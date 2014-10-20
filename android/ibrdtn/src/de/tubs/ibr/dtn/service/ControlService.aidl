/*
 * ControlService.aidl
 * 
 * Copyright (C) 2014 IBR, TU Braunschweig
 *
 * Written-by: Johannes Morgenroth <morgenroth@ibr.cs.tu-bs.de>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

package de.tubs.ibr.dtn.service;

interface ControlService {
	/**
	 * Returns whether Wi-Fi P2P is active or not
	 */
	boolean isP2pActive();
	
	/**
	 * Enable / disable P2P extension
	 */
	void setP2pEnabled(boolean val);
	
	/**
	 * Returns whether BT LE is active or not
	 */
	boolean isBleActive();
	
	/**
	 * Enable / disable BT LE extension
	 */
	void setBleEnabled(boolean val);
	
	/**
	 * Get the version of the daemon
	 * @returns A array with the version at position 0 and the build number at position 1
	 */
	String[] getVersion();
	
	/**
	 * @returns A bundle with the current stats
	 */
	Bundle getStats();
}
