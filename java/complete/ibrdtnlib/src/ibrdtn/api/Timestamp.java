/*
 * Timestamp.java
 * 
 * Copyright (C) 2011 IBR, TU Braunschweig
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
package ibrdtn.api;

import java.util.Calendar;
import java.util.Date;
import java.util.TimeZone;

public class Timestamp {
	
	public final static Long TIMEVAL_CONVERSION = 946684800L;
	
	private Calendar calendar = Calendar.getInstance(TimeZone.getTimeZone("GMT"));
	
	public Timestamp(Date d)
	{
		calendar.setTime(d);
	}
	
	public Timestamp(Long dtntimestamp)
	{
		Long utc_timestamp = (dtntimestamp + Timestamp.TIMEVAL_CONVERSION) * 1000;
		calendar.setTimeInMillis(utc_timestamp);
	}
	
	public Date getDate()
	{
		return calendar.getTime();
	}
	
	public Long getValue()
	{
		return (getDate().getTime() / 1000) - Timestamp.TIMEVAL_CONVERSION;
	}
}
