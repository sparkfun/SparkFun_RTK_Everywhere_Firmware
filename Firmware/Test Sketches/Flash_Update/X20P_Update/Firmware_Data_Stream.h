//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// Firmware_Data_Stream.h
//
// Declare class to stream firmware data
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

#ifndef __FIRMWARE_DATA_STREAM_H__
#define __FIRMWARE_DATA_STREAM_H__

class Firmware_Data_Stream : public NetworkClient
{
  private:
    const uint8_t * _buffer;
    const size_t _size;
    size_t _pos;

  public:
    Firmware_Data_Stream(const uint8_t * buffer, size_t size) : _buffer(buffer), _size(size), _pos(0)
    {
    }

    void init() { _pos = 0; }
    virtual uint8_t connected() override { return 1; }
    virtual int available() override { return _size - _pos; }
    virtual int read() override { return (_pos < _size) ? _buffer[_pos++] : -1; }
    virtual int read(uint8_t *buf, size_t size)
    {
        if (available() < size)
            return -1;
        memcpy(buf, &_buffer[_pos], size);
        _pos += size;
        return size;
    }
    virtual int peek() override { return (_pos < _size) ? _buffer[_pos] : -1; }
    virtual void flush() override {}
    virtual size_t write(uint8_t val) override { return 0; }
};

#endif  // __FIRMWARE_DATA_STREAM_H__
