require 'mkmf'
dir_config('leon', [ File.expand_path(File.dirname(__FILE__)) ], [])
create_makefile('leon')
