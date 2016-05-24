module LEON
  module Pure
    @@checkers = []
    @@types = Array.new 256
    def self.define_type(constant, funcs = {})
      if not @@types[constant] @@types[constant] = {}
      check_splice(constant)
      if funcs.has_key? :check && funcs[:check].class == Proc
        @@checkers.push({ constant: constant, detect: funcs[:check])
      end
      if funcs.has_key? :encode && funcs[:encode].class == Proc
        @@types[constant][:encode] = funcs[:encode]
      end
      if funcs.has_key? :decode && funcs[:decode].class == Proc
        @@types[constant][:decode] = funcs[:decode]
      end
    end
    def self.check_splice(
