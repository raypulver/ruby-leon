require 'spec_helper.rb'
require 'leon'

class Test
  attr_accessor :woop, :doop
  def initialize(w, d)
    @woop, @doop = 5, 6
  end
  def getWoop
    @woop
  end
end

LEON::define_type(0x11, {
  check: Proc.new { |val| val.is_a? Test },
  encode: Proc.new { |buffer, val|
    buffer.write_uint8 val.woop
    buffer.write_uint8 val.doop
  },
  decode: Proc.new { |buffer|
    Test.new buffer.read_uint8, buffer.read_uint8
  }
})

describe LEON do
  it "should serialize and deserialize a custom type" do
    channel = LEON::Channel.new LEON::DYNAMIC
    bounce = channel.decode(channel.encode(Test.new 5, 6))
    expect(bounce.woop).to eql(5)
    expect(bounce.doop).to eql(6)
  end
end
