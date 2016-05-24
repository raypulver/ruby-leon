require 'spec_helper.rb'
require 'leon'
require 'date'

class Test
  attr_accessor :woop, :doop
  def initialize(w, d)
    @woop, @doop = w, d
  end
  def getWoop
    @woop
  end
end

describe LEON do
  it "should define constants" do
    expect(LEON::UINT8).to eql(0xFF)
  end
  it "should be able to read and write doubles" do
    b = LEON::Buffer.new
    b.write_double_le(0, 232.222)
    expect(b.read_double_le(0)).to eql(232.222)
  end
  it "is a bijection" do
    payload = {'a' => 1, 'b' => 2 }
    c = LEON::Channel.new LEON::DYNAMIC
    bounce = c.decode(c.encode(payload))
    expect(bounce).to eql(payload)
  end
  it "can agree on a channel" do
    template = {
      'c' => LEON::STRING,
      'd' => [{
        'a' => LEON::INT8,
        'b' => LEON::BOOLEAN
      }]
    }
    obj = {
      'c' => 'woop',
      'd'=> [{
        'a' => 125,
        'b' => true
      }, {
        'a' => 124,
        'b' => false
      }]
    }
    channel = LEON::Channel.new(template)
    bounce = channel.decode(channel.encode(obj))
    expect(bounce).to eql(obj)
  end

  it "can represent floating point numbers" do
    obj = { 'a' => -232.22, 'b' => -23222.222, 'c' => 232.22 }
    channel = LEON::Channel.new LEON::to_template obj
    expect(channel.decode(channel.encode(obj))).to eql(obj)
  end

  it "can serialize a RegExp" do
    reg = Regexp.new('54', 'i')
    channel = LEON::Channel.new LEON::to_template reg
    bounce = channel.decode(channel.encode(reg))
    expect(bounce.options).to eql(reg.options)
    expect(bounce.source).to eql(reg.source)
  end

  it "converts symbols appropriately" do
    obj = { :woop => 6, :doop => 7 }
    expecting = { 'woop' => 6, 'doop' => 7 }
    c = LEON::Channel.new LEON::DYNAMIC
    bounce = c.decode(c.encode obj)
    expect(bounce).to eql(expecting)
  end

  it "can handle complex data structures" do
    obj = { 'woop' => [ 5.0, 4.0, 232.22 ], 'shoop' => { 'doop' => 'coop', 'loop' => [ 8, 9, 6 ] } }
    c = LEON::Channel.new LEON::to_template obj
    bounce = c.decode(c.encode obj)
    expect(bounce).to eql(obj)
  end

  it "can handle special values" do
    obj = { 'woop' => Time.at(1437343537), 'doop' => Regexp.new('woop', 'i'), 'shoop' => LEON::NaN, 'coop' => LEON::Undefined }
    c = LEON::Channel.new LEON::DYNAMIC
    s = c.encode obj
    bounce = c.decode s
    expect(obj['woop'].class).to eql(Time)
    expect(obj['woop'].to_time.to_i).to eql(1437343537)
    expect(obj['doop'].kind_of? Regexp).to eql(true)
    expect(obj['doop'].source).to eql('woop')
    expect(obj['doop'].options).to eql(1)
    expect(obj['shoop']).to eql(LEON::NaN)
    expect(obj['coop']).to eql(LEON::Undefined)
  end
  it "can serialize Buffers" do
    obj = { 'buf' => LEON::Buffer.new }
    obj['buf'].write_double_le(-232.222)
    c = LEON::Channel.new({ 'buf' => LEON::BUFFER })
    bounce = c.decode c.encode obj
    expect(bounce['buf'] == obj['buf']).to eql(true)
  end
  it "can serialize objects" do
    obj = Test.new 5, 6
    c = LEON::Channel.new LEON::to_template obj
    bounce = c.decode c.encode obj
    expect(bounce.kind_of? Test).to eql(true)
    expect(bounce.woop).to eql(5)
    expect(bounce.doop).to eql(6)
  end
  it "can deduce a template" do
    obj = [{'a' => 'woop'}]
    template = [{'a' => LEON::STRING}]
    expect(LEON::to_template(obj)).to eql(template)
  end
  it "can construct complex templates" do
    obj = [{'a' => true, 'b' => 'woop', 'c' => [ -232222.22, 500, 59999 ] },
      {'a' => true, 'b' => 'doop', 'c' => [-600, 500, 400] },
      {'a' => false, 'b' => 'shoop', 'c' => [1, 2, 3] }]
    template = LEON::to_template(obj)
    expect(template).to eql([{'a' => LEON::BOOLEAN, 'b' => LEON::STRING, 'c' => [LEON::DOUBLE]}])
    channel = LEON::Channel.new(template)
    expected = [{"a"=>true, "b"=>"woop", "c"=>[-232222.22, 500.0, 59999.0]},
       {"a"=>true, "b"=>"doop", "c"=>[-600.0, 500.0, 400.0]},
       {"a"=>false, "b"=>"shoop", "c"=>[1.0, 2.0, 3.0]}]
    expect(channel.decode(channel.encode(obj))).to eql(expected)
  end
  it "can handle templates with symbol keys" do
    temp = { a: LEON::BOOLEAN, b: LEON::DYNAMIC }
    channel = LEON::Channel.new temp
    obj = { a: true, b: false }
    expect(channel.decode channel.encode(obj)).to eql(obj)
  end
  it "can marshal channels" do
    obj = { woop: 5, doop: 6 }
    c = LEON::Channel.new LEON::to_template obj
    s = Marshal.dump c
    us = Marshal.load s
    bounce = us.decode us.encode obj
    expect(bounce).to eql(obj)
  end
  it "can append channels" do
    c1 = LEON::Channel.new
    c2 = LEON::Channel.new
    c3 = LEON::Channel.new c1, c2
    obj = [ { 'a' => 'one', 'b' => 'two' }, 5 ]
    expect(c3.decode c3.encode obj).to eql(obj)
  end
  it "can serialize all numerical types" do
    channel = LEON::Channel.new({
      uc: LEON::UINT8,
      c: LEON::INT8,
      us: LEON::UINT16,
      s: LEON::INT16,
      ui: LEON::UINT32,
      i: LEON::INT32,
      f: LEON::FLOAT,
      d: LEON::DOUBLE
    })
    object = {
      uc: 5,
      c: -5,
      us: 30000,
      s: -30000,
      ui: 500000,
      i: -500000,
      f: 5.0,
      d: 232.22
    }
    expect(channel.decode channel.encode object).to eql(object)
  end
  it "can nest channels" do
    parent = LEON::Channel.new({
      one: LEON::Channel.new(LEON::BUFFER),
      two: LEON::Channel.new(LEON::BUFFER)
    })
    buf = LEON::Buffer.new
    buf.write_double_le(-232.22)
    bounce = parent.decode parent.encode({ one: buf, two: buf })
    expect(bounce[:one] == buf).to eql(true)
    expect(bounce[:two] == buf).to eql(true)
  end
end
