<?php
/*
 * wg_globals.inc
 *
 * part of pfSense (https://www.pfsense.org)
 * Copyright (c) 2021 R. Christian McDonald (https://github.com/theonemcdonald)
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

// pfSense includes
require_once('config.inc');
require_once('globals.inc');

global $g, $wgg;

define('WG_ERROR_IF_NAME',	1);
define('WG_ERROR_IF_CREATE',	2);
define('WG_ERROR_IF_GROUP',	4);
define('WG_ERROR_IF_ADDRESSES',	8);
define('WG_ERROR_IF_UP',	16);
define('WG_ERROR_IF_DOWN',	32);
define('WG_ERROR_IF_SYNC',	64);
define('WG_ERROR_IF_DESTROY',	128);

define('WG_ERROR_SVC_RUNNING',	1);
define('WG_ERROR_SVC_START',	2);
define('WG_ERROR_SVC_STOP',	4);
define('WG_ERROR_SVC_BUILD',	8);

$wgg = array(
	'wg'			=> '/usr/local/bin/wg',
	'ifconfig'		=> '/sbin/ifconfig',
	'pkg'			=> '/usr/sbin/pkg',
	'kldload'		=> '/sbin/kldload',
	'kldunload'		=> '/sbin/kldunload',
	'php'			=> '/usr/local/bin/php',
	'php_wg'		=> '/usr/local/bin/php_wg',
	'wg_includes'		=> '/usr/local/pkg/wireguard',
	'kmod'			=> 'if_wg.ko',
	'pid_path'		=> "{$g['varrun_path']}/wireguardd.pid",
	'subsystem'		=> 'wireguard',
	'applylist'		=> array(
					'tunnels' => "{$g['tmp_path']}/.wireguard_tunnel.apply"),
	'pkg_names'		=> array(
					'pfSense-pkg-WireGuard',
					'wireguard-kmod',
					'wireguard-tools-lite'),
	'conf_path'		=> '/usr/local/etc/wireguard',
	'conf_paths_to_clean'	=> array(
					'/etc/wireguard', 
					'/etc/wg'),
	'xml_path'		=> array('installedpackages', 'wireguard'),
	'xml_paths_to_clean'	=> array(
					array('wireguard')),
	'if_prefix'		=> 'tun_wg',
	'ifgroupentry'		=> array(
					'ifname' => 'WireGuard', 
					'descr' => 'WireGuard Interface Group (DO NOT EDIT/DELETE!)', 
					'members' => null),
	'shellcmdentries'	=> array(
					array(
						'cmd' => 'service wireguardd start',
						'cmdtype' => 'earlyshellcmd',
						'description' => 'WireGuard earlyshellcmd (DO NOT EDIT/DELETE!)')),
	'unboundaclentry'	=> array(
					'aclname' => 'WireGuard',
					'aclaction' => 'allow',
					'description' => 'WireGuard Unbound ACL (DO NOT EDIT/DELETE!)'),
	'handshake_thresholds' 	=> array(
					86400 => array('class' => 'text-danger', 'title' => gettext('Greater than 1 day')),
					300 => array('class' => 'text-warning', 'title' => gettext('Greater than 5 minutes')),
					0 => array('class' => 'text-success', 'title' => gettext('Less than 5 minutes'))),
	'error_flags'		=> array(
					'tunnel' => array(
						WG_ERROR_IF_NAME	=> gettext('Invalid WireGuard tunnel name'),
						WG_ERROR_IF_CREATE	=> gettext('Unable to create WireGuard tunnel interface'),
						WG_ERROR_IF_GROUP	=> gettext('Unable to add WireGuard tunnel interface to the WireGuard interface group'),
						WG_ERROR_IF_ADDRESSES	=> gettext('Unable to update WireGuard tunnel interface addresses'),
						WG_ERROR_IF_UP		=> gettext('Unable to bring up WireGuard tunnel interface'),
						WG_ERROR_IF_DOWN	=> gettext('Unable to bring down WireGuard tunnel interface'),
						WG_ERROR_IF_SYNC	=> gettext('Unable to sync WireGuard tunnel configuration with wg(8)'),
						WG_ERROR_IF_DESTROY	=> gettext('Unable to destroy WireGuard tunnel interface')),
					'service' => array(
						WG_ERROR_SVC_RUNNING	=> gettext('WireGuard service is already running'),
						WG_ERROR_SVC_START	=> gettext('Unable to start WireGuard service'),
						WG_ERROR_SVC_STOP	=> gettext('Unable to stop WireGuard service'),
						WG_ERROR_SVC_BUILD	=> gettext('Unable to build WireGuard tunnel(s)'))),
	'default_mtu'		=> 1420,
	'default_port'		=> 51820,
	'max_port'		=> 65535,
	'max_tunnels'		=> 32768
);

$wgg['wg_daemon'] 		= "{$wgg['php_wg']} -f {$wgg['wg_includes']}/wg_service.inc";

function &array_get_value(array &$array, $parents) {

    $ref = &$array;

    foreach ((array) $parents as $parent) {

        if (is_array($ref) && array_key_exists($parent, $ref)) {

            $ref = &$ref[$parent];

        } else {

            return null;

        }

    }

    return $ref;

}

function array_set_value(array &$array, $parents, $value) {

    $ref = &$array;

    foreach ((array) $parents as $parent) {

        if (isset($ref) && !is_array($ref)) {

            $ref = array();

        }

        $ref = &$ref[$parent];

    }

    $ref = $value;

}

function array_unset_value(&$array, $parents) {

    $key = array_shift($parents);

    if (empty($parents)) {

        unset($array[$key]);

    } else {

        array_unset_value($array[$key], $parents);

    }

}

/*
 * This is a generalized implementation of init_config_arr
 */
function wg_init_config_arr(&$a_config, $keys) {

	$c = &$a_config;

	if (!is_array($keys)) {

		return null;

	}

	foreach ($keys as $k) {

		if (!is_array($c[$k])) {

			$c[$k] = array();

		}

		$c = &$c[$k];

	}
	
}

/*
 * This populates the $wgg with the latest information from config.xml
 */
function wg_globals() {
	global $config, $wgg;

	// Reload config.xml to get any recent changes
	$config = parse_config(true);

	// An array of configuration paths we need
	$conf_paths 	= array(
				'config' => array_merge($wgg['xml_path'], array('config', 0)),
				'tunnels' => array_merge($wgg['xml_path'], array('tunnels', 'item')),
				'peers' => array_merge($wgg['xml_path'], array('peers', 'item')));


	// Need to initialize these config paths as arrays and then assign by reference to $wgg
	foreach ($conf_paths as $key => $path) {

		wg_init_config_arr($config, $path);

		$wgg[$key] = &array_get_value($config, $path);

	}


}

// Call this often to read the latest configuration information
wg_globals();

?>