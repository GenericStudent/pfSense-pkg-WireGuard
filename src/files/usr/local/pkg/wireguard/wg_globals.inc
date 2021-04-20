<?php
/*
 * wg_globals.inc
 *
 * part of pfSense (https://www.pfsense.org)
 * Copyright (c) 2021 R. Christian McDonald
 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the 'License');
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an 'AS IS' BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Global defines

require_once('config.inc');
require_once('globals.inc');

global $wgg;

$wgg = array(
        'wg'                    => '/usr/local/bin/wg',
        'wg_quick'              => '/usr/local/bin/wg-quick',
        'bash'                  => '/usr/local/bin/bash',
        'ifconfig'              => '/sbin/ifconfig',
        'kldstat'               => '/sbin/kldstat',
        'pkg'                   => '/usr/sbin/pkg',
        'kmod'                  => 'if_wg.ko',
        'pkg_name'              => 'wireguard',
        'conf_path'             => '/usr/local/etc/wireguard',
        'conf_paths_to_clean'   => array('/usr/local/etc/wireguard', '/etc/wireguard', '/etc/wg'),
        'xml_conf_tags'         => 'wireguard',
        'if_prefix'             => 'tun_wg',
        'if_group'              => 'WireGuard',
        'ifgroupentry'          => array('ifname' => 'WireGuard', 'descr' => 'Dynamic Group for WireGuard', 'members' => null),
        'default_mtu'           => 1420,
        'default_port'          => 51820,
        'max_port'              => 65535,
        'max_tunnels'           => 32768
);

/*
 * This populates the $wgg with the latest information from config.xml
 */
function wg_globals() {
        global $config, $wgg;

        // Reload config.xml to get any recent changes
	$config = parse_config(true);

        // Need to initialize as arrays
        init_config_arr(array('installedpackages', $wgg['pkg_name'], 'config'));
        init_config_arr(array('installedpackages', $wgg['pkg_name'], 'tunnel'));

        // Reference assignment for reading and writing back to the configure through $wgg
        $wgg['config'] = &$config['installedpackages'][$wgg['pkg_name']]['config'][0];
        $wgg['tunnels'] = &$config['installedpackages'][$wgg['pkg_name']]['tunnel'];

}

// Call this to read the latest configuration when this file is included
wg_globals();

?>