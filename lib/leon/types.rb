require 'date'

module LEON
  UINT8 = 0xff
  INT8 = 0xfe
  UINT16 = 0xfd
  INT16 = 0xfc
  UINT32 = 0xfb
  INT32 = 0xfa
  FLOAT = 0xf9
  DOUBLE = 0xf8
  ARRAY = 0xf6
  OBJECT = 0xf5
  STRING = 0xf4
  BOOLEAN = 0xf1
  NULL = 0xf0
  UNDEFINED = 0xef
  DATE = 0xee
  BUFFER = 0xed
  REGEXP = 0xec
  NAN = 0xeb
  INFINITY = 0xe7
  MINUS_INFINITY = 0xe6
  class Undefined
  end
  class NaN
  end
  class Infinity
  end
  class MinusInfinity
  end
  def self.type_check(v)
    for checker in @@type_checkers
      return checker.constant if checker.detect.call v
    end
    case v.class
      when NilClass
        return self::NULL
      when TrueClass
        return self::BOOLEAN + 2
      when FalseClass
        return self::BOOLEAN + 1
      when self::NaN
        return self::NAN
      when self::Undefined
        return self::UNDEFINED
      when Date, Time, DateTime
        return self::DATE
      when self::Buffer
        return self::BUFFER
      when Regexp
        return self::REGEXP
      when Array
        return self::ARRAY
      when Hash
        return self::OBJECT
      when String
        return self::STRING
      when Symbol
        return self::SYMBOL
      when Fixnum, Bignum
        if v < 0
          v = v.abs
          if v <= 128
            return self::INT8
          elsif v <= 32768
            return self::INT16
          elsif v <= 2147483648
            return self::INT32
          end
          return self::DOUBLE
        end
        if v < 256
          return self::UINT8
        end
        if v < 65536
          return self::UINT16
        end
        if v < 4294967296
          return self::UINT32
        end
        return self::DOUBLE
      when Float 
        if v === Float::INFINITY
          return self::INFINITY
        elsif v === -1/0.0
          return self::MINUS_INFINITY
        if [v].pack('e').unpack('e')[0] == v
          return self::FLOAT
        end
        return self::DOUBLE
      else
        if v.kind_of? Array
          return self::ARRAY
        else return self::OBJECT
        end
      end
    end
  end
end

