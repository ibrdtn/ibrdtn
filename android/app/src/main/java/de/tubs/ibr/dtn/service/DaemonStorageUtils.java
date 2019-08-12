/*
 * StorageUtils.java
 * 
 * Copyright (C) 2013 IBR, TU Braunschweig
 *
 * Written-by: Dominik Schürmann <dominik@dominikschuermann.de>
 * 	           Johannes Morgenroth <morgenroth@ibr.cs.tu-bs.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
package de.tubs.ibr.dtn.service;

import java.io.File;

import android.content.Context;
import android.os.Environment;

public class DaemonStorageUtils {
	public static void clearBlobPath(Context context) {
		// storage path
		File blobPath = DaemonStorageUtils.getBlobPath(context);
		if (blobPath == null) return;
		
		// flush storage path
		File[] files = blobPath.listFiles();
		if (files != null) {
			for (File f : files) {
				f.delete();
			}
		}
	}
	
	public static File getBlobPath(Context context) {
		File cachedir = context.getCacheDir();
		if (cachedir == null) return null;
		return new File(cachedir.getPath() + File.separatorChar + "blob");
	}
	
	public static File getStoragePath(Context context)
	{
		if (Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED))
		{
			File externalStorage = context.getExternalFilesDir(null);
			if (externalStorage == null) return null;
			return new File(externalStorage.getPath() + File.separatorChar + "storage");
		}

		return null;
	}
	
	public static File getLogPath(Context context)
	{
		if (Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED))
		{
			File externalStorage = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
			if (externalStorage == null) return null;
			return new File(externalStorage.getPath() + File.separatorChar + "logs");
		}

		return null;
	}
	
	public static File getSecurityPath(Context context)
	{
		return new File(context.getFilesDir().getPath() + File.separatorChar + "bpsec");
	}
	
	public static String getConfigurationFile(Context context) {
		return context.getFilesDir().getPath() + File.separatorChar + "config";
	}
}
