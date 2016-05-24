begin
  require "#{File.dirname(__FILE__)}/leon.so"
  require "#{File.dirname(__FILE__)}/leon/leon"
  module LEON
    include LEON::Ext
    def is_external?
      true
    end
  end
rescue LoadError
  require "#{File.dirname(__FILE__)}/leon/leon"
  module LEON
    include LEON::Pure
    def is_external?
      false
    end
  end
end
