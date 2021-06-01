<?php
/*
 * wg_service.inc
 *
 * part of pfSense (https://www.pfsense.org)
 * Copyright (c) 2021 R. Christian McDonald (https://github.com/theonemcdonald)
 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


// pfSense includes
require_once('config.inc');
require_once('globals.inc');
require_once('gwlb.inc');
require_once('util.inc');
require_once('services.inc');
require_once('service-utils.inc');

// WireGuard includes
require_once('wireguard/wg.inc');

if (isset($argv[1])) {

	ignore_user_abort(true);

	set_time_limit(0);

	pcntl_async_signals(true);

	if (!wg_is_cli()) {

		// Bail out if we aren't in the CLI...
		die("FATAL: This script can only be started through the CLI.\n");

	}

	if (PHP_BINARY != $wgg['php_wg']) {

		// Bail out if we aren't running under under the correct executable...
		die("FATAL: This script can only be executed by {$wgg['php_wg']}.\n");

	}

	// Should we serialize the return output?
	$serialize = (isset($argv[2]) && strtolower($argv[2]) == 'serialize');

	switch (strtolower($argv[1])) {
		
		case 'start':

			$ret_code = wg_service_cli_start($serialize);
	
			exit($ret_code);
			
			break;

		case 'stop':
	
			$ret_code = wg_service_cli_stop($serialize);
	
			exit($ret_code);

			break;

		default:

			exit(1);

			break;

	}

}

// This is a wrapper for safely calling from web frontend
function wg_service_fpm_restart() {
	global $wgg;

	$start_status = $stop_status = array();

	if (wg_is_service_running()) {

		$stop_status = wg_service_fpm_stop();

		if ($stop_status['ret_code'] <> 0) {

			return $stop_status;

		}

	}

	$start_status = wg_service_fpm_start();

	return $start_status;

}

// This is a wrapper for safely calling from web frontend
function wg_service_fpm_stop() {
	global $wgg;

	$serialized_output = $ret_output = array();

	// Invokes service stop with serialization flag so we can cleanly report back to the web frontend.
	exec("{$wgg['wg_daemon']} stop serialize", $serialized_output);

	if ($ret_output = unserialize($serialized_output[0])) {

		return $ret_output;

	}

	// Consumers expect an array...
	return array();

}

// Only for calling service stop from the CLI
function wg_service_cli_stop($serialize_output = true) {
	global $wgg;

	$ret_code = 0;

	if (!wg_is_cli()) {

		$ret_code |= WG_ERROR_SVC_STOP;

		wg_service_error_handler($ret_code, $serialize_output);

		// Terminate early...
		return $ret_code;

	}

	// Need to wait here for just a second...
	if (killbypid($wgg['pid_path'], 1) <> 0) {

		$ret_code |= WG_ERROR_SVC_STOP;

		wg_service_error_handler($ret_code, $serialize_output);

	}

	return $ret_code;

}

// This is a wrapper for safely calling from the web frontend
function wg_service_fpm_start() {
	global $wgg;

	$serialized_output = $ret_output = array();

	// Invokes service start with serialization flag so we can cleanly report back to the web frontend
	exec("{$wgg['wg_daemon']} start serialize", $serialized_output, $ret_code);

	// Catch unserialization results before returning
	if ($ret_output = unserialize($serialized_output[0])) {

		return $ret_output;

	}

	// Consumers expect an array type
	return array();

}

// Only for calling service start from the CLI
function wg_service_cli_start($serialize_output = true) {
	global $g, $wgg;

	// Set the process name
	cli_set_process_title('WireGuard service');
	
	$ret_code = 0;

	if (!wg_is_cli()) {

		$ret_code |= WG_ERROR_SVC_START;

		wg_service_error_handler($ret_code, $serialize_output);

		return $ret_code;

	}

	if (wg_is_service_running()) {

		$ret_code |= WG_ERROR_SVC_RUNNING;

		wg_service_error_handler($ret_code, $serialize_output);

		return $ret_code;

	}

	// Register the service environment and lock early to ensure singletons
	wg_register_service_env(false);

	if (platform_booting()) {

		// This absolutely needs to be a file on disk to remain accessible between process instances
		mark_subsystem_dirty('wireguard_firststart');

		// Output during booting must be STDERR for some reason
		fwrite(STDERR, "Configuring WireGuard tunnels...");

		// Supresses ifconfig spew 
		mute_kernel_msgs();

	}

	// Build all (null) the tunnels
	$sync_all_status = wg_tunnel_sync(null);

	if ($sync_all_status['ret_code'] <> 0 ) {

		$ret_code |= WG_ERROR_SVC_BUILD;

	}

	if (platform_booting()) {

		unmute_kernel_msgs();

		fwrite(STDERR, "done. \n");

		return $ret_code;

	}

	// Now, the initial fork...
	$newpid = pcntl_fork();

	if ($newpid === -1) {

		$ret_code |= WG_ERROR_SVC_START;

		wg_destroy_tunnels();

		wg_service_error_handler($ret_code, $serialize_output);

		return $ret_code;

	} elseif ($newpid) {

		wg_service_error_handler($ret_code, $serialize_output, array('sync_all_status' => $sync_all_status));

		return $ret_code;

	}

	// Now become the session leader
	if (posix_setsid() < 0) {

		wg_destroy_tunnels();

		return 1;

	}

	// The second fork...
	$newpid = pcntl_fork();

	if ($newpid === -1) {

		wg_destroy_tunnels();

		return 1;

	} elseif ($newpid) {

		// Reap the child process below...
		pcntl_waitpid($newpid, $status);

		// Boilerplate if we want to trap service ret codes and halt...
		$ret_code = pcntl_wexitstatus($status);

		if ($ret_code <> 0) {

			return $ret_code;
		}

		// Move on to the actual daemon
		wg_service_daemon();

		// We shouldn't be here...
		return 0;

	} else {

		// We don't need to restart additional services ifw we just finished booting...
		if (is_subsystem_dirty('wireguard_firststart')) {

			clear_subsystem_dirty('wireguard_firststart');

			return 0;

		}

		// Restart any additional services
		$ret_code = wg_restart_extra_services();

		return $ret_code;

	}

	// We shouldn't be here...
	return 1;

}

function wg_restart_extra_services() {

	// dpinger
	setup_gateways_monitor();

	// unbound
	services_unbound_configure();

	// pf
	filter_configure();

	// This is currently just a best effort attempt...
	return 0;

}

// Main WireGuard service loop
function wg_service_daemon() {
	global $wgg;

	// Re-register the service environment
	wg_register_service_env(true);

	// Now that we are properly daemonized, register the service signal handlers
	wg_register_daemon_sig_handler();

	// Attempt to load the kmod, required to run the service without any tunnels configured
	if (!is_module_loaded($wgg['kmod'])) {

		$kmod = $wgg['kmod'];

		wg_escapeshellarg($kmod);

		mwexec("{$wgg['kldload']} {$kmod}");

	}

	// Main WireGuard service loop
	while (true) {

		// The whole point of this daemon...
		if (!is_module_loaded($wgg['kmod'])) {

			break;

		}

		// Wait a bit before trying again
		sleep(1);

	}

	// Execute SIGTERM handler to exit gracefully
	wg_daemon_sig_handler(SIGTERM);

}

function wg_deregister_service_env() {
	global $h_lock, $wgg;

	if (!is_null($h_lock)) {

		// Attempt to release exclusive lock
		@flock($h_lock, LOCK_UN);

		// Attempt to close file handler
		@fclose($h_lock);

	}

	// Attempt to delete PID file
	unlink_if_exists($wgg['pid_path']);

}

function wg_register_service_env($close_fdio = false) {
	global $h_lock, $wgg;

	wg_deregister_service_env();

	if ($h_lock = fopen($wgg['pid_path'], 'a+')) {

		flock($h_lock, LOCK_EX);

		ftruncate($h_lock, 0);

		fseek($h_lock, 0, 0);

		fwrite($h_lock, getmypid());

		fflush($h_lock);

	}

	if ($close_fdio) {

		fclose(STDIN);

		fclose(STDOUT);

		fclose(STDERR);

	}

}

function wg_register_daemon_sig_handler() {

	pcntl_signal(SIGTERM, 'wg_daemon_sig_handler');

}

function wg_daemon_sig_handler($signo) {

	switch ($signo) {

		case SIGTERM:

			// Teardown any tunnels and unload the module
			wg_destroy_tunnels();

			// Cleanup the service environment
			wg_deregister_service_env();

			// We are done...
			exit(0);

			break;

		default:

			break;

	}

}

function wg_service_error_handler($ret_code, $serialize_output = true, $extras = array()) {
	global $wgg;

	$errors = array();

	foreach ($wgg['error_flags']['service'] as $error_mask => $error_text) {

		if (($ret_code & $error_mask) > 0) {
	
			if ($serialize_output) {

				$errors[$error_mask] = $error_text;

			} else {

				fwrite(STDERR, "{$error_text}\n");

			}

		}

	}

	if ($serialize_output) {

		$ret_array = array_merge(array('ret_code' => $ret_code, 'errors' => $errors), $extras);

		$ret_serialized = serialize($ret_array);

		fwrite(STDOUT, "{$ret_serialized}\n");

	}

	return $ret_code;

}

// Check if we are in CLI or not
function wg_is_cli() {

	return (PHP_SAPI == 'cli');

}

// Checks if the service is running
function wg_is_service_running() {
	global $wgg;

	if (!($h_lock = @fopen($wgg['pid_path'], 'r')) || !file_exists($wgg['pid_path'])) {

		return false;

	}

	$not_running = flock($h_lock, LOCK_EX | LOCK_NB, $wouldblock);

	if ($not_running) {
	
		flock($h_lock, LOCK_UN);

	}

	$pid = fgets($h_lock);
	
	fclose($h_lock);

	// Another trick to test if a process is running
	$sig_test = posix_kill($pid, 0);

	return (!$not_running || $wouldblock || $sig_test);

}

?>