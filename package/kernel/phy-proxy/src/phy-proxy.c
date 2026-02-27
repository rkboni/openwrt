// SPDX-License-Identifier: GPL-2.0
/*
 * Virtual Switch PHY Network Device Driver
 * 
 * Creates virtual network devices that:
 * - Show real PHY link status from switch-connected PHYs
 * - Forward data to/from parent netdevs (single or bonded)
 * - Automatically create and manage bond devices for multi-parent configs
 * - Fully functional network interfaces compatible with all tools
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/phy.h>
#include <linux/of.h>
#include <linux/of_mdio.h>
#include <linux/of_net.h>
#include <linux/of_platform.h>
#include <linux/rtnetlink.h>
#include <net/bonding.h>

#define DRIVER_NAME "phy-proxy"

struct phy_proxy_priv {
	struct platform_device *pdev;
	struct phy_device *phydev;

	/* Parent netdevs that actually carry traffic */
	struct net_device **parent_devs;
	unsigned int num_parents;

	/* For multi-parent: create internal bond */
	struct net_device *bond_dev;
	char bond_mode[32];
	char bond_xmit_hash[32];

	/* Statistics */
	struct pcpu_sw_netstats __percpu *stats;

	u8 mac_addr[ETH_ALEN];
};

/* Fast-path TX: forward to parent or bond */
static netdev_tx_t phy_proxy_start_xmit(struct sk_buff *skb,
		struct net_device *dev)
{
	struct phy_proxy_priv *priv = netdev_priv(dev);
	struct net_device *target = NULL;
	struct pcpu_sw_netstats *stats;
	netdev_tx_t ret;

	/* Determine target device */
	if (priv->bond_dev) {
		target = priv->bond_dev;
	} else if (priv->num_parents == 1 && priv->parent_devs[0]) {
		target = priv->parent_devs[0];
	}

	if (!target || !(target->flags & IFF_UP)) {
		dev_kfree_skb(skb);
		dev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	/* Update stats - kernel 6.x uses u64_stats_t wrapper */
	stats = this_cpu_ptr(priv->stats);
	u64_stats_update_begin(&stats->syncp);
	u64_stats_inc(&stats->tx_packets);
	u64_stats_add(&stats->tx_bytes, skb->len);
	u64_stats_update_end(&stats->syncp);

	/* Forward packet to target device */
	skb->dev = target;
	skb->priority = 0;

	ret = dev_queue_xmit(skb);
	if (ret != NET_XMIT_SUCCESS)
		dev->stats.tx_errors++;

	return NETDEV_TX_OK;
}

/* RX handler: intercept packets from parent devices */
static rx_handler_result_t phy_proxy_rx_handler(struct sk_buff **pskb)
{
	struct sk_buff *skb = *pskb;
	struct phy_proxy_priv *priv;
	struct net_device *dev;
	struct pcpu_sw_netstats *stats;

	priv = rcu_dereference(skb->dev->rx_handler_data);
	if (!priv)
		return RX_HANDLER_PASS;

	dev = priv->pdev->dev.driver_data;
	if (!dev || !(dev->flags & IFF_UP))
		return RX_HANDLER_PASS;

	/* Consume outgoing frames - don't let them back up the stack */
	if (skb->pkt_type == PACKET_OUTGOING)
		return RX_HANDLER_CONSUMED;

	/* Consume our own frames to prevent loops */
	if (ether_addr_equal(eth_hdr(skb)->h_source, dev->dev_addr))
		return RX_HANDLER_CONSUMED;

	/* Clone if shared */
	if (skb_shared(skb)) {
		struct sk_buff *nskb = skb_clone(skb, GFP_ATOMIC);
		if (!nskb)
			return RX_HANDLER_CONSUMED;
		skb = nskb;
		*pskb = nskb;
	}

	/* Update stats */
	stats = this_cpu_ptr(priv->stats);
	u64_stats_update_begin(&stats->syncp);
	u64_stats_inc(&stats->rx_packets);
	u64_stats_add(&stats->rx_bytes, skb->len);
	u64_stats_update_end(&stats->syncp);

	/* Redirect to our virtual device */
	skb->dev = dev;
	skb->pkt_type = PACKET_HOST;

	return RX_HANDLER_ANOTHER;
}

static void phy_proxy_adjust_link(struct net_device *dev)
{
	struct phy_proxy_priv *priv = netdev_priv(dev);
	struct phy_device *phydev = priv->phydev;

	if (!phydev)
		return;

	if (phydev->link) {
		netif_carrier_on(dev);
		netdev_info(dev, "PHY Link up - %d Mbps, %s duplex\n",
				phydev->speed,
				phydev->duplex == DUPLEX_FULL ? "full" : "half");
	} else {
		netif_carrier_off(dev);
		netdev_info(dev, "PHY Link down\n");
	}
}

static int phy_proxy_open(struct net_device *dev)
{
	struct phy_proxy_priv *priv = netdev_priv(dev);
	int i, err;

	/* If we have a bond, just register RX handler - bond manages its slaves */
	if (priv->bond_dev) {
		/* Open the bond when lan2 comes up - it's our transport layer */
		if (!(priv->bond_dev->flags & IFF_UP)) {
			err = dev_open(priv->bond_dev, NULL);
			if (err) {
				netdev_err(dev, "Failed to open bond: %d\n", err);
				return err;
			}
		}

		/* Register RX handler on the bond */
		err = netdev_rx_handler_register(priv->bond_dev,
				phy_proxy_rx_handler, priv);
		if (err) {
			netdev_err(dev, "Failed to register RX handler on bond: %d\n", err);
			dev_close(priv->bond_dev);  /* Clean up */
			return err;
		}
	} else {
		/* Single parent - we manage it */
		for (i = 0; i < priv->num_parents; i++) {
			if (!priv->parent_devs[i])
				continue;

			/* Bring up parent if needed */
			if (!(priv->parent_devs[i]->flags & IFF_UP)) {
				dev_open(priv->parent_devs[i], NULL);
			}

			err = netdev_rx_handler_register(priv->parent_devs[i],
					phy_proxy_rx_handler, priv);
			if (err) {
				netdev_warn(dev, "Failed to register RX handler: %d\n", err);
			}
		}
	}

	/* Start PHY */
	if (priv->phydev) {
		phy_start(priv->phydev);

		if (priv->phydev->link)
			netif_carrier_on(dev);
		else
			netif_carrier_off(dev);
	}

	return 0;
}

static int phy_proxy_stop(struct net_device *dev)
{
	struct phy_proxy_priv *priv = netdev_priv(dev);
	int i;

	/* Stop PHY */
	if (priv->phydev)
		phy_stop(priv->phydev);

	/* Unregister RX handlers */
	if (priv->bond_dev) {
		netdev_rx_handler_unregister(priv->bond_dev);
	} else {
		for (i = 0; i < priv->num_parents; i++) {
			if (priv->parent_devs[i])
				netdev_rx_handler_unregister(priv->parent_devs[i]);
		}
	}

	netif_carrier_off(dev);

	return 0;
}

static void phy_proxy_get_stats64(struct net_device *dev,
		struct rtnl_link_stats64 *stats)
{
	struct phy_proxy_priv *priv = netdev_priv(dev);
	unsigned int start;
	int i;

	for_each_possible_cpu(i) {
		struct pcpu_sw_netstats *pstats = per_cpu_ptr(priv->stats, i);
		u64 rx_packets, rx_bytes, tx_packets, tx_bytes;

		do {
			start = u64_stats_fetch_begin(&pstats->syncp);
			rx_packets = u64_stats_read(&pstats->rx_packets);
			rx_bytes = u64_stats_read(&pstats->rx_bytes);
			tx_packets = u64_stats_read(&pstats->tx_packets);
			tx_bytes = u64_stats_read(&pstats->tx_bytes);
		} while (u64_stats_fetch_retry(&pstats->syncp, start));

		stats->rx_packets += rx_packets;
		stats->rx_bytes += rx_bytes;
		stats->tx_packets += tx_packets;
		stats->tx_bytes += tx_bytes;
	}
}

static void phy_proxy_change_rx_flags(struct net_device *dev, int flags)
{
    struct phy_proxy_priv *priv = netdev_priv(dev);
    struct net_device *target = priv->bond_dev ?
                                priv->bond_dev : priv->parent_devs[0];

    if (!target)
        return;

    if (flags & IFF_PROMISC) {
        if (dev->flags & IFF_PROMISC)
            dev_set_promiscuity(target, 1);
        else
            dev_set_promiscuity(target, -1);
    }

    if (flags & IFF_ALLMULTI) {
        if (dev->flags & IFF_ALLMULTI)
            dev_set_allmulti(target, 1);
        else
            dev_set_allmulti(target, -1);
    }
}

static const struct net_device_ops phy_proxy_netdev_ops = {
		.ndo_open		= phy_proxy_open,
		.ndo_stop		= phy_proxy_stop,
		.ndo_start_xmit		= phy_proxy_start_xmit,
		.ndo_get_stats64	= phy_proxy_get_stats64,
		.ndo_change_rx_flags    = phy_proxy_change_rx_flags,
		.ndo_validate_addr	= eth_validate_addr,
		.ndo_set_mac_address	= eth_mac_addr,
};

static void phy_proxy_setup(struct net_device *dev)
{
	ether_setup(dev);
	dev->netdev_ops = &phy_proxy_netdev_ops;
	dev->needs_free_netdev = true;
	dev->priv_flags |= IFF_LIVE_ADDR_CHANGE;

	/* Don't set NETIF_F_LLTX - removed in newer kernels */
	dev->hw_features = dev->features;
}

static int phy_proxy_parse_parents(struct phy_proxy_priv *priv,
		struct device_node *np)
{
	struct device_node *parent_np;
	struct platform_device *parent_pdev;
	int i, count;

	/* Count parent-netdevs references */
	count = of_count_phandle_with_args(np, "parent-netdevs", NULL);
	if (count <= 0) {
		dev_err(&priv->pdev->dev, "No parent-netdevs specified\n");
		return -EINVAL;
	}

	priv->num_parents = count;
	priv->parent_devs = devm_kcalloc(&priv->pdev->dev, count,
			sizeof(*priv->parent_devs),
			GFP_KERNEL);
	if (!priv->parent_devs)
		return -ENOMEM;

	/* Get parent netdev references */
	for (i = 0; i < count; i++) {
		parent_np = of_parse_phandle(np, "parent-netdevs", i);
		if (!parent_np)
			continue;

		/* Find the platform device for this gmac - use of_find_device_by_node from of_platform.h */
		parent_pdev = of_find_device_by_node(parent_np);
		of_node_put(parent_np);

		if (parent_pdev) {
			/* Get the netdev from platform device */
			priv->parent_devs[i] = platform_get_drvdata(parent_pdev);
			if (!priv->parent_devs[i]) {
				platform_device_put(parent_pdev);
				return -EPROBE_DEFER;
			}
			if(count == 1){
				dev_info(&priv->pdev->dev, "mac = %s\n", priv->parent_devs[i]->name);
			} else {
				dev_info(&priv->pdev->dev, "mac%d = %s\n",
						i, priv->parent_devs[i]->name);
			}
			platform_device_put(parent_pdev);
		}
	}

	return 0;
}

static int phy_proxy_create_bond(struct phy_proxy_priv *priv,
		const char *bond_name)
{
	struct net_device *bond_dev;
	struct bonding *bond;
	struct bond_opt_value mode_val, xmit_val, carrier_val, updelay_val, downdelay_val;
	struct netlink_ext_ack extack = {};
	int err, i;
	int bond_mode = BOND_MODE_XOR;
	int xmit_policy = BOND_XMIT_POLICY_LAYER23;

	/* Parse bond mode from DT */
	if (strlen(priv->bond_mode) > 0) {
		if (strcmp(priv->bond_mode, "balance-rr") == 0)
			bond_mode = BOND_MODE_ROUNDROBIN;
		else if (strcmp(priv->bond_mode, "active-backup") == 0)
			bond_mode = BOND_MODE_ACTIVEBACKUP;
		else if (strcmp(priv->bond_mode, "balance-xor") == 0)
			bond_mode = BOND_MODE_XOR;
		else if (strcmp(priv->bond_mode, "broadcast") == 0)
			bond_mode = BOND_MODE_BROADCAST;
		else if (strcmp(priv->bond_mode, "802.3ad") == 0)
			bond_mode = BOND_MODE_8023AD;
		else if (strcmp(priv->bond_mode, "balance-tlb") == 0)
			bond_mode = BOND_MODE_TLB;
		else if (strcmp(priv->bond_mode, "balance-alb") == 0)
			bond_mode = BOND_MODE_ALB;
	}

	/* Parse xmit hash policy from DT */
	if (strlen(priv->bond_xmit_hash) > 0) {
		if (strcmp(priv->bond_xmit_hash, "layer2") == 0)
			xmit_policy = BOND_XMIT_POLICY_LAYER2;
		else if (strcmp(priv->bond_xmit_hash, "layer3+4") == 0)
			xmit_policy = BOND_XMIT_POLICY_LAYER34;
		else if (strcmp(priv->bond_xmit_hash, "layer2+3") == 0)
			xmit_policy = BOND_XMIT_POLICY_LAYER23;
		else if (strcmp(priv->bond_xmit_hash, "encap2+3") == 0)
			xmit_policy = BOND_XMIT_POLICY_ENCAP23;
		else if (strcmp(priv->bond_xmit_hash, "encap3+4") == 0)
			xmit_policy = BOND_XMIT_POLICY_ENCAP34;
	}

	/* Allocate bond device */
	bond_dev = alloc_netdev_mq(sizeof(struct bonding),
			bond_name,
			NET_NAME_UNKNOWN,
			bond_setup,
			1);
	if (!bond_dev) {
		dev_err(&priv->pdev->dev, "Failed to allocate bond device\n");
		return -ENOMEM;
	}

	/* Register the bond device first */
	err = register_netdevice(bond_dev);
	if (err) {
		dev_err(&priv->pdev->dev, "Failed to register bond: %d\n", err);
		goto err_free_bond;
	}

	bond = netdev_priv(bond_dev);
	bond_dev->priv_flags |= IFF_DONT_BRIDGE;

	bond_opt_initval(&mode_val, bond_mode);
	err = __bond_opt_set(bond, BOND_OPT_MODE, &mode_val, NULL, &extack);
	if (err) {
		dev_err(&priv->pdev->dev, "Failed to set bond mode: %d\n", err);
		if (extack._msg)
			dev_err(&priv->pdev->dev, "  %s\n", extack._msg);
		goto err_unregister_bond;
	}

	bond_opt_initval(&xmit_val, xmit_policy);
	__bond_opt_set(bond, BOND_OPT_XMIT_HASH, &xmit_val, NULL, &extack);

	bond_opt_initval(&carrier_val, 1);
	__bond_opt_set(bond, BOND_OPT_USE_CARRIER, &carrier_val, NULL, &extack);

	bond_opt_initval(&updelay_val, 200);
	__bond_opt_set(bond, BOND_OPT_UPDELAY, &updelay_val, NULL, &extack);

	bond_opt_initval(&downdelay_val, 200);
	__bond_opt_set(bond, BOND_OPT_DOWNDELAY, &downdelay_val, NULL, &extack);

	dev_info(&priv->pdev->dev, "bond %s: mode=%d, xmit_hash=%d\n",
			bond_dev->name, bond_mode, xmit_policy);

	/* Enslave parent devices - bond will take MAC from first slave */
	for (i = 0; i < priv->num_parents; i++) {
		if (!priv->parent_devs[i])
			continue;

		if (priv->parent_devs[i]->flags & IFF_UP)
			dev_close(priv->parent_devs[i]);

		err = bond_enslave(bond_dev, priv->parent_devs[i], &extack);
		if (err) {
			dev_err(&priv->pdev->dev,
					"Failed to enslave %s to bond: %d\n",
					priv->parent_devs[i]->name, err);
			if (extack._msg)
				dev_err(&priv->pdev->dev, "  %s\n", extack._msg);
			continue;
		}

		dev_info(&priv->pdev->dev, "%s -> %s\n",
				priv->parent_devs[i]->name, bond_dev->name);
	}

	priv->bond_dev = bond_dev;
	return 0;

err_unregister_bond:
	unregister_netdevice(bond_dev);
	return err;

err_free_bond:
	free_netdev(bond_dev);
	return err;
}

static int phy_proxy_get_link_ksettings(struct net_device *dev,
		struct ethtool_link_ksettings *cmd)
{
	struct phy_proxy_priv *priv = netdev_priv(dev);

	if (!priv->phydev)
		return -ENODEV;

	phy_ethtool_ksettings_get(priv->phydev, cmd);

	return 0;
}

static int phy_proxy_set_link_ksettings(struct net_device *dev,
		const struct ethtool_link_ksettings *cmd)
{
	struct phy_proxy_priv *priv = netdev_priv(dev);

	if (!priv->phydev)
		return -ENODEV;

	return phy_ethtool_ksettings_set(priv->phydev, cmd);
}

static void phy_proxy_get_drvinfo(struct net_device *dev,
		struct ethtool_drvinfo *info)
{
	strscpy(info->driver, DRIVER_NAME, sizeof(info->driver));
	strscpy(info->version, "1.0", sizeof(info->version));
}

static u32 phy_proxy_get_link(struct net_device *dev)
{
	struct phy_proxy_priv *priv = netdev_priv(dev);

	if (!priv->phydev)
		return 0;

	return priv->phydev->link;
}

static int phy_proxy_get_sset_count(struct net_device *dev, int sset)
{
    struct phy_proxy_priv *priv = netdev_priv(dev);
    struct net_device *target = priv->bond_dev ?
                                priv->bond_dev : priv->parent_devs[0];

    if (!target || !target->ethtool_ops->get_sset_count)
        return -EOPNOTSUPP;

    return target->ethtool_ops->get_sset_count(target, sset);
}

static void phy_proxy_get_strings(struct net_device *dev,
                                   u32 stringset, u8 *data)
{
    struct phy_proxy_priv *priv = netdev_priv(dev);
    struct net_device *target = priv->bond_dev ?
                                priv->bond_dev : priv->parent_devs[0];

    if (!target || !target->ethtool_ops->get_strings)
        return;

    target->ethtool_ops->get_strings(target, stringset, data);
}

static void phy_proxy_get_ethtool_stats(struct net_device *dev,
                                         struct ethtool_stats *stats,
                                         u64 *data)
{
    struct phy_proxy_priv *priv = netdev_priv(dev);
    struct net_device *target = priv->bond_dev ?
                                priv->bond_dev : priv->parent_devs[0];

    if (!target || !target->ethtool_ops->get_ethtool_stats)
        return;

    target->ethtool_ops->get_ethtool_stats(target, stats, data);
}

static const struct ethtool_ops phy_proxy_ethtool_ops = {
		.get_drvinfo		= phy_proxy_get_drvinfo,
		.get_link			= phy_proxy_get_link,
		.get_link_ksettings	= phy_proxy_get_link_ksettings,
		.set_link_ksettings	= phy_proxy_set_link_ksettings,
	    .get_sset_count     = phy_proxy_get_sset_count,
	    .get_strings        = phy_proxy_get_strings,
	    .get_ethtool_stats  = phy_proxy_get_ethtool_stats,
};

static int phy_proxy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *phy_node;
	struct phy_proxy_priv *priv;
	phy_interface_t iface = PHY_INTERFACE_MODE_NA;
	struct net_device *netdev;
	const char *label;
	const char *bond_name = NULL;
	int err;

	if (!np) {
		dev_err(dev, "No device tree node\n");
		return -EINVAL;
	}

	/* Get label for netdev name */
	err = of_property_read_string(np, "label", &label);
	if (err)
		label = "lan%d";

	/* Allocate netdev */
	netdev = alloc_netdev(sizeof(*priv), label, NET_NAME_UNKNOWN,
			phy_proxy_setup);
	if (!netdev)
		return -ENOMEM;

	priv = netdev_priv(netdev);
	priv->pdev = pdev;
	platform_set_drvdata(pdev, netdev);
	pdev->dev.driver_data = netdev;
	SET_NETDEV_DEV(netdev, dev);

	netdev->ethtool_ops = &phy_proxy_ethtool_ops;

	/* Allocate per-CPU stats */
	priv->stats = netdev_alloc_pcpu_stats(struct pcpu_sw_netstats);
	if (!priv->stats) {
		err = -ENOMEM;
		goto err_free_netdev;
	}

	/* Parse parent netdevs */
	err = phy_proxy_parse_parents(priv, np);
	if (err)
		goto err_free_stats;

	/* Get PHY reference and read interface mode before of_node_put */
	phy_node = of_parse_phandle(np, "phy-handle", 0);
	if (!phy_node) {
		dev_err(dev, "No phy-handle specified\n");
		err = -EINVAL;
		goto err_free_stats;
	}

	/* Read interface mode from PHY DT node before releasing it */
	of_get_phy_mode(phy_node, &iface);

	priv->phydev = of_phy_find_device(phy_node);
	of_node_put(phy_node);

	if (!priv->phydev) {
		dev_info(dev, "PHY device not ready, deferring\n");
		err = -EPROBE_DEFER;
		goto err_free_stats;
	}

	/* Parse bond configuration if multiple parents */
	if (priv->num_parents > 1) {
		of_property_read_string(np, "bond-mode",
				(const char **)&priv->bond_mode);
		of_property_read_string(np, "bond-xmit-hash",
				(const char **)&priv->bond_xmit_hash);
		of_property_read_string(np, "bond-name", &bond_name);

		if (!bond_name)
			bond_name = "bond-cpu";

		/* Create bond - it will take its MAC from first slave naturally */
		rtnl_lock();
		err = phy_proxy_create_bond(priv, bond_name);
		rtnl_unlock();

		if (err) {
			dev_err(dev, "Failed to create bond: %d\n", err);
			goto err_put_phy;
		}

		/* Take MAC from bond which got it from first slave */
		ether_addr_copy(priv->mac_addr, priv->bond_dev->dev_addr);
	} else {
		/* Single parent - get MAC from nvmem/DT */
		u8 mac[ETH_ALEN];
		err = of_get_mac_address(np, mac);
		if (err == -EPROBE_DEFER)
			goto err_put_phy;
		if (!err && is_valid_ether_addr(mac))
			ether_addr_copy(priv->mac_addr, mac);
		else
			eth_random_addr(priv->mac_addr);
	}

	/* Set netdev MAC from bond (multi-parent) or nvmem (single parent) */
	eth_hw_addr_set(netdev, priv->mac_addr);

	/* Register netdev FIRST (before attaching PHY to avoid deadlock) */
	err = register_netdev(netdev);
	if (err) {
		dev_err(dev, "Failed to register netdev: %d\n", err);
		goto err_destroy_bond;
	}

	/* Attach PHY with interface mode from PHY DT node */
	err = phy_attach_direct(netdev, priv->phydev, 0, iface);
	if (err) {
		dev_err(dev, "Failed to attach PHY: %d\n", err);
		goto err_unregister_netdev;
	}

	priv->phydev->adjust_link = phy_proxy_adjust_link;
	phy_support_asym_pause(priv->phydev);

	/* Enable autoneg - bootloader may have left PHY in forced mode */
	priv->phydev->autoneg = AUTONEG_ENABLE;
	linkmode_copy(priv->phydev->advertising, priv->phydev->supported);

	dev_info(dev, "Created %s with MAC %pM for %s PHY (iface=%d, %d parent%s%s)\n",
			netdev->name, priv->mac_addr, phydev_name(priv->phydev),
			iface, priv->num_parents,
			priv->num_parents > 1 ? "s" : "",
			priv->bond_dev ? ", using bond" : "");

	return 0;

err_unregister_netdev:
	unregister_netdev(netdev);
	return err;

err_destroy_bond:
	if (priv->bond_dev) {
		rtnl_lock();
		unregister_netdevice(priv->bond_dev);
		rtnl_unlock();
	}
err_put_phy:
	put_device(&priv->phydev->mdio.dev);
err_free_stats:
	free_percpu(priv->stats);
err_free_netdev:
	free_netdev(netdev);
	return err;
}

static void phy_proxy_remove(struct platform_device *pdev)
{
	struct net_device *netdev = platform_get_drvdata(pdev);
	struct phy_proxy_priv *priv = netdev_priv(netdev);

	unregister_netdev(netdev);

	if (priv->phydev) {
		phy_detach(priv->phydev);
		put_device(&priv->phydev->mdio.dev);
	}

	/* Destroy bond if we created it */
	if (priv->bond_dev) {
		rtnl_lock();
		unregister_netdevice(priv->bond_dev);
		rtnl_unlock();
		/* Bond device freed automatically */
	}

	free_percpu(priv->stats);
}

static const struct of_device_id phy_proxy_of_match[] = {
		{ .compatible = "virtual,phy-proxy" },
		{ }
};
MODULE_DEVICE_TABLE(of, phy_proxy_of_match);

static struct platform_driver phy_proxy_driver = {
		.probe = phy_proxy_probe,
		.remove_new = phy_proxy_remove,
		.driver = {
				.name = DRIVER_NAME,
				.of_match_table = phy_proxy_of_match,
		},
};

module_platform_driver(phy_proxy_driver);

MODULE_AUTHOR("Chris Lockwood");
MODULE_DESCRIPTION("Virtual PHY Proxy Network Device");
MODULE_LICENSE("GPL v2");
