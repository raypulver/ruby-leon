require 'spec_helper'
require 'leon'

describe LEON::Buffer do
  it "appends bytes" do
    b = LEON::Buffer.new
    b.write_uint8(40)
    b.write_uint8(50)
    b.write_uint8(60)
    expect(b.read_uint8(2)).to eql(60)
    expect(b.read_uint8(1)).to eql(50)
  end
  it "reads and writes uint8 values" do
    b = LEON::Buffer.new
    b.write_uint8 5
    b.write_uint8 6
    expect(b.read_uint8 0).to eql(5)
    expect(b.read_uint8 1).to eql(6)
  end
  it "reads and writes int8 values" do
    b = LEON::Buffer.new
    b.write_int8 -120
    b.write_int8 -100
    expect(b.read_int8 0).to eql(-120)
    expect(b.read_int8 1).to eql(-100)
  end
  it "reads and writes uint16 values" do
    b = LEON::Buffer.new
    b.write_uint16_le 50000
    b.write_uint16_be 40000
    expect(b.read_uint16_le 0).to eql(50000)
    expect(b.read_uint16_be 2).to eql(40000)
  end
  it "reads and writes int16 values" do
    b = LEON::Buffer.new
    b.write_int16_le -30000
    b.write_int16_be -20000
    expect(b.read_int16_le 0).to eql(-30000)
    expect(b.read_int16_be 2).to eql(-20000)
  end
  it "reads and writes uint32 values" do
    b = LEON::Buffer.new
    b.write_uint32_le 500000
    b.write_uint32_be 600000
    expect(b.read_uint32_le 0).to eql(500000)
    expect(b.read_uint32_be 4).to eql(600000)
  end
  it "reads and writes int32 values" do
    b = LEON::Buffer.new
    b.write_int32_le -500000
    b.write_int32_be -600000
    expect(b.read_int32_le 0).to eql(-500000)
    expect(b.read_int32_be 4).to eql(-600000)
  end
  it "reads and writes floats" do
    b = LEON::Buffer.new
    b.write_float_le -5.0
    b.write_float_be -6.0
#    expect(b.read_float_le 0).to eql(-5.0)
    expect(b.read_float_be 4).to eql(-6.0)
  end
  it "reads and writes doubles" do
    b = LEON::Buffer.new
    b.write_double_le -232.22
    b.write_double_be -2322.2
    expect(b.read_double_le 0).to eql(-232.22)
    expect(b.read_double_be 8).to eql(-2322.2)
  end
  it "can be constructed from an array" do
    b = LEON::Buffer.new [255, 0, 5]
    expect(b[0]).to eql(255)
    expect(b[1]).to eql(0)
    expect(b[2]).to eql(5)
  end
  it "can be constructed from a string" do
    b = LEON::Buffer.new "woop"
    expect(b.to_s).to eql("woop")
  end
  it "can be constructed from a hex string" do
    b = LEON::Buffer.from_hex "0xff"
    expect(b.length).to eql(1)
    expect(b[0]).to eql(0xff);
    b = LEON::Buffer.from_hex "f"
    expect(b.length).to eql(1)
    expect(b[0]).to eql(0x0f);
  end 
  it "can convert to/from base64" do
    b = LEON::Buffer.new [255, 255, 255, 255]
    b64 = b.to_base64
    bounce = LEON::Buffer.from_base64 b64
    expect(bounce == b).to eql(true)
  end
  it "adjusts sizes appropriately" do
    b = LEON::Buffer.new "woop"
    v = b.make_view(2)
    b[4] = 5
    v.length = 3
    expect(v.length).to eql(3)
    expect(v[2]).to eql(5)
  end
end
