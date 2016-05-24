require 'types'

module LEON
  module Pure
    class BufferIterator
      public
      def initialize(buf, opts = {})
        @buffer = buf
        @position = opts[:position] || 0
        @offset = opts[:offset] || 0
      end
      def read_uint8
        @position += 1
        return @buffer.read_uint8(@position - 1)
      end
      def read_uint8
        @position += 1
        return @buffer.read_int8(@position - 1)
			end
			def read_uint16_le
				@position += 2
				return @buffer.read_uint16_le(@position - 2)
			end
			def read_uint16_be
				@position += 2
				return @buffer.read_uint16_be(@position - 2)
			end
			def read_int16_le
				@position += 2
				return @buffer.read_int16_le(@position - 2)
			end
			def read_int16_be
				@position += 2
				return @buffer.read_int16_be(@position - 2)
			end
			def read_uint32_le
				@position += 4
				return @buffer.read_uint32_le(@position - 4)
			end
			def read_uint32_be
				@position += 4
				return @buffer.read_uint32_be(@position - 4)
			end
			def read_int32_le
				@position += 4
				return @buffer.read_int32_le(@position - 4)
			end
			def read_int32_be
				@position += 4
				return @buffer.read_int32_be(@position - 4)
			end
			def read_float_le
				@position += 4
				return @buffer.read_float_le(@position - 4)
			end
			def read_float_be
				@position += 4
				return @buffer.read_float_be(@position - 4)
			end
			def read_double_le
				@position += 8
				return @buffer.read_double_le(@position - 8)
			end
			def read_double_be
				@position += 8
				return @buffer.read_double_be(@position - 8)
			end
			private
      def read_len
        first = read_uint8()
        if first == 0xff
          if (second = read_uint8) == 0xff
            return 0xff
          else
            return (second << 8) | read_uint8
          end
        elsif first == 0xfe
          if (second = read_uint8) == 0xff
            return 0xfe
          else
            third = read_uint8
            return second << 24 | third << 16 | read_uint16_be
          end
        else
          return first
        end
      end
      def read_dynamic
        type = read_uint8
        if 
        case type
          when LEON::UINT8
            return read_uint8
          when LEON::INT8
            return read_int8
          when LEON::UINT16
            return read_uint16_le
      def write_len(val)
        if val < 0xfe
          write_uint8 val
        elsif val == 0xfe
          write_uint16_be 0xfeff
        elsif val == 0xff
          write_uint16_be 0xffff
        elsif val < 0x10000
          write_uint8 0xff
          write_uint16_be val
        elsif val < 0x100000000
          write_uint8 0xfe
          write_uint32_be val
        else
          raise ArgumentError, "Item too large."
        end
      end
            
			def readValue(type)
				if type === Constants::UNSIGNED_CHAR
					return readUInt8()
				end
				if type === Constants::CHAR
					return readInt8()
				end
				if type === Constants::UNSIGNED_SHORT
					return readUInt16()
				end
				if type === Constants::SHORT
					return readInt16()
				end
				if type === Constants::UNSIGNED_INT
					return readUInt32()
				end
				if type === Constants::INT
					return readInt32()
				end
				if type === Constants::FLOAT
					return readFloat()
				end
				if type === Constants::DOUBLE
					return readDouble()
				end
			end
		end
	end
