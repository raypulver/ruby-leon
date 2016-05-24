require 'types'

module LEON
  module Pure
    class Buffer
      attr_accessor :buffer
      private
      def normalize_or_raise_with_data(index, args)
        retval = []
        if args.length == 1
          if index < 0
            retval.push(index + @buffer.length)
            if retval[0] < 0
              raise RangeError, "Index #{index} out of range."
            end
          else
            retval.push index
          end
          retval.push args[0]
        elsif not args.length
          retval.push(@buffer.length)
          retval.push(index)
        else
          raise ArgumentError "Must supply 1 or 2 arguments."
        end
      end
      def normalize_or_raise(index, bytes)
        if index < 0
          retval = index + @buffer.length
          if retval < 0
            raise RangeError, "Index #{index} out of range."
          end
        else
          retval = index
        end
        if retval + bytes > @buffer.length
          raise RangeError, "Tried to read past end of buffer."
        end
        return retval
      end
      def maybe_fill(index, bytes)
        if index + bytes > @buffer.length
          @buffer += "\0" * (index + bytes - @buffer.length)
        end
      end
      public
      def initialize(*args)
        if args.empty? || args[0].class != String
          @buffer = ''
        else
          @buffer = args[0]
        end
      end 
      def self.complement(v, bits)
        return (v ^ ((1 << bits) - 1)) + 1
      end
      def write_uint8(index, *args)
        index, data = normalize_or_raise_with_data(index, args)
        maybe_fill(index, 1)
        if data.class == Fixnum
          @buffer[index] = data.chr
        elsif data.class == Float
          @buffer[index] = data.to_i.chr
        else
          raise TypeError "Must supply a Fixnum."
        end
      end
      def write_int8(index, *args)
        index, data = normalize_or_raise_with_data(index, args)
        maybe_fill(index, 1)
        if data.class == Fixnum
          val = data
        elsif data.class == Float
          val = data.to_i
        else
          raise TypeError "Must supply a Fixnum."
        end
        if val < 0
          @buffer[index] = self.class::complement(-val.chr, 8)
        else
          @buffer[index] = val.chr
        end
      end
      def write_uint16_le(index, *args)    
        index, data = normalize_or_raise_with_data(index, args)
        maybe_fill(index, 2)
        if data.class == Float
          data = data.to_i
        end
        if data.class != Fixnum
          raise TypeError "Must supply a Fixnum."
        end
        @buffer[index] = (data & 0xFF).chr
        @buffer[index + 1] = (data >> 8).chr
        return self
      end
      def write_uint16_be(index, *args)    
        index, data = normalize_or_raise_with_data(index, args)
        maybe_fill(index, 2)
        if data.class == Float
          data = data.to_i
        end
        if data.class != Fixnum
          raise TypeError "Must supply a Fixnum."
        end
        @buffer[index] = (data & 0xFF).chr
        @buffer[index + 1] = (data >> 8).chr
        return self
      end
      def write_int16_le(index, *args)
        index, data = normalize_or_raise_with_data(index, args)
        if data < 0
          return write_uint16_le(index, self.class::complement(-data, 16))
        else
          return write_uint16_le(index, data)
        end
      end
      def write_int16_be(index, *args)
        index, data = normalize_or_raise_with_data(index, args)
        if data < 0
          return write_uint16_be(index, self.class::complement(-data, 16))
        else
          return write_uint16_be(index, data)
        end
      end
      def write_uint32_le(index, *args)    
        index, data = normalize_or_raise_with_data(index, args)
        maybe_fill(index, 4)
        if data.class == Float
          data = data.to_i
        end
        if data.class != Fixnum
          raise TypeError "Must supply a Fixnum."
        end
        @buffer[index] = (v & 0xFF).chr
        @buffer[index + 1] = ((v >> 8) & 0xFF).chr
        @buffer[index + 2] = ((v >> 16) & 0xFF).chr
        @buffer[index + 3] = (v >> 24).chr
        return self
      end
      def write_uint32_be(index, *args)    
        index, data = normalize_or_raise_with_data(index, args)
        maybe_fill(index, 4)
        if data.class == Float
          data = data.to_i
        end
        if data.class != Fixnum
          raise TypeError "Must supply a Fixnum."
        end
        @buffer[index + 3] = (v & 0xFF).chr
        @buffer[index + 2] = ((v >> 8) & 0xFF).chr
        @buffer[index + 1] = ((v >> 16) & 0xFF).chr
        @buffer[index] = (v >> 24).chr
        return self
      end
      def write_int32_le(index, *args)
        index, data = normalize_or_raise_with_data(index, args)
        if data < 0
          return write_uint32_le(index, self.class::complement(-data, 32))
        else
          return write_uint32_le(index, data)
        end
      end
      def write_int32_be(index, *args)
        index, data = normalize_or_raise_with_data(index, args)
        if data < 0
          return write_uint32_be(index, self.class::complement(-data, 32))
        else
          return write_uint32_be(index, data)
        end
      end
      def write_float_le(index, *args)    
        index, data = normalize_or_raise_with_data(index, args)
        maybe_fill(index, 4)
        [v].pack('e').chars.each_with_index { |v, i| @buffer[index + i] = v }
        return self
      end
      def write_float_be(index, *args)    
        index, data = normalize_or_raise_with_data(index, args)
        maybe_fill(index, 4)
        [v].pack('g').chars.each_with_index { |v, i| @buffer[index + i] = v }
        return self
      end
      def write_double_le(index, *args)    
        index, data = normalize_or_raise_with_data(index, args)
        maybe_fill(index, 8)
        [v].pack('E').chars.each_with_index { |v, i| @buffer[index + i] = v }
        return self
      end
      def write_double_be(index, *args)    
        index, data = normalize_or_raise_with_data(index, args)
        maybe_fill(index, 8)
        [v].pack('G').chars.each_with_index { |v, i| @buffer[index + i] = v }
        return self
      end
      def read_uint8(index)
        index = normalize_or_raise(index, 1)
        return @buffer[index].ord
      end
      def read_int8(index)
        retval = read_uint8(index)
        if index & 0x80
          return -self.class::complement(retval, 8)
        else
          return retval
        end
      end
      def read_uint16_le(index)
        index = normalize_or_raise(index, 2)
        return @buffer[index].ord | (@buffer[index].ord << 8)
      end
      def read_uint16_be(index)
        index = normalize_or_raise(index, 2)
        return @buffer[index + 1].ord | (@buffer[index].ord << 8)
      end
      def read_int16_le(index)
        retval = read_uint16_le(index)
        if retval & 0x8000
          return -self.class::complement(retval, 16)
        else
          return retval
        end
      end
      def read_int16_be(index)
        retval = read_uint16_be(index)
        if retval & 0x8000
          return -self.class::complement(retval, 16)
        else
          return retval
        end
      end
      def read_uint32_le(index)
        index = normalize_or_raise(index, 4)
        return @buffer[index].ord | (@buffer[index + 1].ord << 8) | (@buffer[index + 2].ord << 16) | (@buffer[index + 3].ord << 24)
      end
      def read_uint32_be(index)
        index = normalize_or_raise(index, 4)
        return @buffer[index + 3].ord | (@buffer[index + 2].ord << 8) | (@buffer[index + 1].ord << 16) | (@buffer[index].ord << 24)
      end
      def read_int32_le(index)
        retval = read_uint32_le(index)
        if retval & 0x80000000
          return -self.class::complement(retval, 32)
        else
          return retval
        end
      end
      def read_int32_be(index)
        retval = read_uint32_be(index)
        if retval & 0x80000000
          return -self.class::complement(retval, 32)
        else
          return retval
        end
      end
      def read_float_le(index)
        index = normalize_or_raise(index, 4)
        @buffer[index...(index + 4)].unpack('e')
      end
      def read_float_be(index)
        index = normalize_or_raise(index, 4)
        @buffer[index...(index + 4)].unpack('g')
      end
      def read_double_le(index)
        index = normalize_or_raise(index, 8)
        @buffer[index...(index + 8)].unpack('E')
      end
      def read_double_be(index)
        index = normalize_or_raise(index, 8)
        @buffer[index...(index + 8)].unpack('G')
      end
      def to_s
        return @buffer
      end
      def length
        return @buffer.length
      def length=(value)
        if value < 0
          raise ArgumentError, "Length cannot be negative."
        end
        if value < @buffer.length
          @buffer = @buffer[0...value]
        elsif value > @buffer.length
          maybe_fill(value, 0)
        end
        return self
      end
      def ==(other)
        return @buffer == other.buffer
      end
    end
  end
end
