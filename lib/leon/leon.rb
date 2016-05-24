require 'buffer-iterator'
module LEON
  this_module = self
  class Channel
    def initialize(spec)
      @spec = spec
    end
    def encode(payload)
      this_module::BufferIterator.new.write_value(payload, @sorted).buffer.to_s
    end
    def decode(payload)
      this_module::BufferIterator.new(payload).read_value(payload, @sorted)
    end
  end
  def self.decode(str)
    self.new.decode str
  end
  def self.encode(payload)
    self.new.encode(payload)
  end
  def self.type_to_str(type)
    case type
      when Constants::UNSIGNED_CHAR
        return "unsigned char"
      when Constants::CHAR
        return "char"
      when Constants::UNSIGNED_SHORT
        return "unsigned short"
      when Constants::SHORT
        return "short"
      when Constants::UNSIGNED_INT
        return "unsigned int"
      when Constants::INT
        return "int"
      when Constants::FLOAT
        return "float"
      when Constants::DOUBLE
        return "double"
      when Constants::STRING
        return "string"
      when Constants::BOOLEAN
        return "boolean"
      when Constants::NULL
        return "null"
      when Constants::UNDEFINED
        return "undefined"
      when Constants::OBJECT, Constants::NATIVE_OBJECT
        return "object"
      when Constants::ARRAY
        return "array"
      when Constants::DATE
        return "date"
      when Constants::BUFFER
        return "buffer"
      when Constants::REGEXP
        return "RegExp"
      when Constants::NAN
        return "NaN"
      when Constants::INFINITY
        return "infinity"
      when Constants::MINUS_INFINITY
        return "minus infinity"
    end
  end
  def self.type_gcd(arr)
    type = LEON::type_check(arr[0])
    if type === Constants::BOOLEAN + 1
      type = Constants::BOOLEAN
    end
    case type
      when Constants::ARRAY
        return [ self.type_gcd(arr[0]) ]
      when Constants::OBJECT
        ret = Hash.new
        arr[0].each { |k, v|
          ret[k] = self.type_gcd(self.pluck(arr, k))
        }
        return ret
      when Constants::NATIVE_OBJECT
        ret = Hash.new
        vars = arr[0].instance_variables.map { |v|
          v.to_sym.sub("@", "")
        }
        vars.each { |v|
          ret[v] = self.type_gcd(self.pluck(arr, v))
        }
        return ret
      when Constants::UNSIGNED_CHAR, Constants::CHAR, Constants::UNSIGNED_SHORT, Constants::SHORT, Constants::UNSIGNED_INT, Constants::INT, Constants::FLOAT, Constants::DOUBLE
        highestMagnitude = arr[0].abs
        if arr[0] < 0
          sign = 1
        else
          sign = 0
        end
        if type === Constants::FLOAT or type === Constants::DOUBLE
          fp = 1
        else
          fp = 0
        end
        for i in 1...arr.length
          type = LEON::type_check(arr[i])
          if not arr[i].kind_of? Float and not arr[i].kind_of? Fixnum
            raise "Expected a numerical value but got #{self.type_to_str(type)}."
          end
          if arr[i].abs > highestMagnitude
            highestMagnitude = arr[1]
          end
          if arr[i].ceil != arr[i]
            fp = 1
          else
            fp = 0
          end
        end
        return self.type_check((fp ? Float((sign ? -highestMagnitude : highestMagnitude )) : (sign ? -highestMagnitude : highestMagnitude )))
      else
        for i in 1...(arr.length)
          thisType = self.type_check(arr[i])
          if thisType === Constants::BOOLEAN + 1
            thisType = Constants::BOOLEAN
          end
          if type != thisType
            raise "Was expecting a #{self.type_to_str(type)} but got a #{self.type_to_str(thisType)}."
          end
        end
        return type
    end
    return type
  end
  def self.pluck(arr, prop)
    ret = Array.new
    if prop.kind_of? Symbol
      prop = prop.to_s
    end
    for i in 0...(arr.length)
      if arr[i].kind_of? Hash
        if not arr[i].has_key? prop
          if not arr[i].has_key? prop.to_sym
            raise "Object #{i} in array has no such key \"#{prop}.\""
          else
            ret.push arr[i][prop.to_sym]
          end
        else
          ret.push arr[i][prop]
        end
      elsif arr[i].kind_of? Object
        begin
          ret.push arr[i].send(prop)
        rescue
          raise "Object #{i} in array has no such property \"#{prop}.\""
        end
      end
    end
    return ret
  end
  def self.to_template(payload)
    type = self.type_check(payload)
    case type
      when Constants::ARRAY
        return [self.type_gcd(payload)]
      when Constants::OBJECT
        ret = Hash.new
        payload.each { |k, v|
          ret[k] = self.to_template(v)
        }
        return ret
      when Constants::NATIVE_OBJECT
        ret = Hash.new
        payload.instance_variables.each { |k, v|
          ret[k] = self.to_template(v)
        }
      when Constants::BOOLEAN + 1
        return Constants::BOOLEAN
      else
        return type
    end
  end
end
class Object
  def to_leon()
    LEON::generate(self)
  end
end
