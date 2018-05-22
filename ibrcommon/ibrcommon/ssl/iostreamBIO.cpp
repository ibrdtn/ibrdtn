/*
 * iostreamBIO.cpp
 *
 * Copyright (C) 2011 IBR, TU Braunschweig
 *
 * Written-by: Stephen Roettger <roettger@ibr.cs.tu-bs.de>
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

#include "ibrcommon/ssl/iostreamBIO.h"

#include "ibrcommon/Logger.h"

#include "openssl_compat.h"
#include <openssl/err.h>

namespace ibrcommon
{

static const int ERR_BUF_SIZE = 256;

const char * const iostreamBIO::name = "iostreamBIO";

/* callback functions for openssl */
static int bwrite(BIO *bio, const char *buf, int len);
static int bread(BIO *bio, char *buf, int len);
//static int bputs(BIO *bio, const char *);
//static int bgets(BIO *bio, char *, int);
static long ctrl(BIO *bio, int cmd, long num, void *ptr);
static int create(BIO *bio);
//static int destroy(BIO *bio);
//static long (*callback_ctrl)(BIO *, int, bio_info_cb *);

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
BIO_METHOD * BIO_iostream_method()
{
	static BIO_METHOD *iostream_method = NULL;
	if (iostream_method) {
		iostream_method = BIO_meth_new(iostreamBIO::type, iostreamBIO::name);
		BIO_meth_set_write(iostream_method, bwrite);
		BIO_meth_set_read(iostream_method, bread);
		BIO_meth_set_ctrl(iostream_method, ctrl);
		BIO_meth_set_create(iostream_method, create);
	}
	return iostream_method;
}
#else
static BIO_METHOD iostream_method =
{
		iostreamBIO::type,
		iostreamBIO::name,
		bwrite,
		bread,
		NULL,//bputs
		NULL,//bgets
		ctrl,
		create,
		NULL,//destroy,
		NULL//callback_ctrl
};
BIO_METHOD * BIO_iostream_method()
{
	return &iostream_method;
}
#endif

iostreamBIO::iostreamBIO(std::iostream *stream)
	:	_stream(stream)
{
	/* create BIO */
	_bio = BIO_new(BIO_iostream_method());
	if(!_bio){
		/* creation failed, throw exception */
		char err_buf[ERR_BUF_SIZE];
        ERR_error_string_n(ERR_get_error(), err_buf, ERR_BUF_SIZE);
        err_buf[ERR_BUF_SIZE - 1] = '\0';
        IBRCOMMON_LOGGER_TAG("iostreamBIO", critical) << "BIO creation failed: " << err_buf << IBRCOMMON_LOGGER_ENDL;
		throw BIOException(err_buf);
	}

	/* save the iostream in the bio object */
	BIO_set_data(_bio, (void *) stream);
}

BIO * iostreamBIO::getBIO(){
	return _bio;
}

static int create(BIO *bio)
{
	BIO_set_data(bio, NULL);
	BIO_set_shutdown(bio, 1);
	BIO_set_init(bio, 1);
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	/* from bss_mem.c (openssl):
	 * bio->num is used to hold the value to return on 'empty', if it is
	 * 0, should_retry is not set
	 *
	 * => -1 means the caller can retry, 0: retry is useless
	 * it is set to 0 since the underlying stream is blocking
	 */
	bio->num= 0;
#endif

	return 1;
}



static long ctrl(BIO *bio, int cmd, long  num, void *)
{
	long ret;
	std::iostream *stream = reinterpret_cast<std::iostream*>(BIO_get_data(bio));

	IBRCOMMON_LOGGER_DEBUG_TAG("iostreamBIO", 90) << "ctrl called, cmd: " << cmd << ", num: " << num << "." << IBRCOMMON_LOGGER_ENDL;

	switch(cmd){
	case BIO_CTRL_PUSH:
	case BIO_CTRL_POP:
		ret = 0;
		break;
//	case BIO_CTRL_GET_CLOSE:
//		ret = bio->shutdown;
//		break;
//	case BIO_CTRL_SET_CLOSE:
//		bio->shutdown = (int)num;
//		ret = 1;
//		break;
	case BIO_CTRL_FLUSH:
		/* try to flush the underlying stream */
		ret = 1;
		try{
			stream->flush();
		} catch(std::ios_base::failure &ex){
			/* ignore, the badbit is checked instead */
		}
//		catch(ConnectionClosedException &ex){
//			throw;
//		}
		if(stream->bad()){
			IBRCOMMON_LOGGER_DEBUG_TAG("iostreamBIO", 20) << "underlying Stream went bad while flushing." << IBRCOMMON_LOGGER_ENDL;
			ret = 0;
		}
		break;
	default:
		IBRCOMMON_LOGGER_TAG("iostreamBIO", warning) << "ctrl called with unhandled cmd: " << cmd << "." << IBRCOMMON_LOGGER_ENDL;
		ret = 0;
		break;
	}

	return ret;
}



static int bread(BIO *bio, char *buf, int len)
{
	std::iostream *stream = reinterpret_cast<std::iostream*>(BIO_get_data(bio));
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	int num_bytes = 0;
#else
	int num_bytes = bio->num;
#endif

	try{
		/* make sure to read at least 1 byte and then read as much as we can */
		num_bytes = static_cast<int>( stream->read(buf, 1).readsome(buf+1, len-1) + 1 );
	} catch(std::ios_base::failure &ex){
		/* ignore, bio->num will be returned and indicate the error */
	}
//	catch(ConnectionClosedException &ex){
//		throw; //this exception will be catched at higher layers
//	}

	return num_bytes;
}



static int bwrite(BIO *bio, const char *buf, int len)
{
	if(len == 0){
		return 0;
	}
	std::iostream *stream = reinterpret_cast<std::iostream*>(BIO_get_data(bio));

	/* write the data */
	try{
		stream->write(buf, len);
	} catch(std::ios_base::failure &ex){
		/* ignore, the badbit is checked instead */
	}
//	catch(ConnectionClosedException &ex){
//		throw;
//	}
	if(stream->bad()){
		IBRCOMMON_LOGGER_DEBUG_TAG("iostreamBIO", 20) << "underlying Stream went bad while writing." << IBRCOMMON_LOGGER_ENDL;
		return 0;
	}

	/* flush the underlying stream */
	try{
		stream->flush();
	} catch(std::ios_base::failure &ex){
		/* ignore, the badbit is checked instead */
	}
//	catch(ConnectionClosedException &ex){
//		throw;
//	}
	if(stream->bad()){
		IBRCOMMON_LOGGER_DEBUG_TAG("iostreamBIO", 20) << "underlying Stream went bad while flushing (bwrite)." << IBRCOMMON_LOGGER_ENDL;
		return 0;
	}

	return len;
}

}
