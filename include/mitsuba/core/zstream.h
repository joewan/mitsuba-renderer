/*
    This file is part of Mitsuba, a physically based rendering system.

    Copyright (c) 2007-2010 by Wenzel Jakob and others.

    Mitsuba is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License Version 3
    as published by the Free Software Foundation.

    Mitsuba is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#if !defined(__ZSTREAM_H)
#define __ZSTREAM_H

#include <mitsuba/mitsuba.h>
#include <zlib.h>
#define ZSTREAM_BUFSIZE 16384

MTS_NAMESPACE_BEGIN

/** \brief Transparent compression/decompression stream based on ZLIB.
 * \ingroup libcore
 */
class MTS_EXPORT_CORE ZStream : public Stream {
public:
	/// Create a new compression stream
	ZStream(Stream *childStream, int level = Z_DEFAULT_COMPRESSION);

	/// Return the child stream of this compression stream
	inline const Stream *getChildStream() const { return m_childStream.get(); }

	/// Return the child stream of this compression stream
	inline Stream *getChildStream() { return m_childStream; }

	/* Stream implementation */
	void read(void *ptr, size_t size);
	void write(const void *ptr, size_t size);
	void setPos(size_t pos);
	size_t getPos() const;
	size_t getSize() const;
	void truncate(size_t size);
	void flush();
	bool canWrite() const;
	bool canRead() const;

	/// Return a string representation
	std::string toString() const;

	MTS_DECLARE_CLASS()
protected:
	// \brief Virtual destructor
	virtual ~ZStream();
private:
	ref<Stream> m_childStream;
	z_stream m_deflateStream, m_inflateStream;
	uint8_t m_deflateBuffer[ZSTREAM_BUFSIZE];
	uint8_t m_inflateBuffer[ZSTREAM_BUFSIZE];
	bool m_didWrite;
};

MTS_NAMESPACE_END

#endif /* __ZSTREAM_H */
