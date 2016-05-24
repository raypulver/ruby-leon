# -*- ruby -*-
require 'rubygems'
require 'shellwords'

gem 'hoe'
require 'hoe'
Hoe.plugin :debugging
Hoe.plugin :git
Hoe.plugin :gemspec
Hoe.plugin :bundler
Hoe.add_include_dirs '.'

ENV['LANG'] = "en_US.UTF-8"

def java?
  /java/ === RUBY_PLATFORM
end

CrossRuby = Struct.new(:version, :host) {
  def ver
    @ver ||= version[/\A[^-]+/]
  end

  def minor_ver
    @minor_ver ||= ver[/\A\d\.\d(?=\.)/]
  end

  def api_ver_suffix
    case minor_ver
    when nil
      raise "unsupported version: #{ver}"
    when '1.9'
      '191'
    else
      minor_ver.delete('.') << '0'
    end
  end

  def platform
    @platform ||=
      case host
      when /\Ax86_64-/
        'x64-mingw32'
      when /\Ai[3-6]86-/
        'x86-mingw32'
      else
        raise "unsupported host: #{host}"
      end
  end

  def tool(name)
    (@binutils_prefix ||=
      case platform
      when 'x64-mingw32'
        'x86_64-w64-mingw32-'
      when 'x86-mingw32'
        'i686-w64-mingw32-'
      end) + name
  end

  def target
    case platform
    when 'x64-mingw32'
      'pei-x86-64'
    when 'x86-mingw32'
      'pei-i386'
    end
  end

  def libruby_dll
    case platform
    when 'x64-mingw32'
      "x64-msvcrt-ruby#{api_ver_suffix}.dll"
    when 'x86-mingw32'
      "msvcrt-ruby#{api_ver_suffix}.dll"
    end
  end

  def dlls
    [
      'kernel32.dll',
      'msvcrt.dll',
      'ws2_32.dll',
      *(case
        when ver >= '2.0.0'
          'user32.dll'
        end),
      libruby_dll
    ]
  end
}

CROSS_RUBIES = File.read('.cross_rubies').lines.flat_map { |line|
  case line
  when /\A([^#]+):([^#]+)/
    CrossRuby.new($1, $2)
  else
    []
  end
}

ENV['RUBY_CC_VERSION'] ||= CROSS_RUBIES.map(&:ver).uniq.join(":")

HOE = Hoe.spec 'leon' do
  developer 'Raymond Pulver IV', 'raymond.pulver_iv@uconn.edu'
  license "MIT"
  self.readme_file  = "README.md"
  self.history_file = ['CHANGELOG', ENV['HLANG'], 'rdoc'].compact.join('.')
  self.extra_rdoc_files = FileList['*.rdoc','ext/leon/*.{c, h}']
  self.clean_globs += [
    'leon.gemspec',
  ]
  self.clean_globs += Dir.glob("ports/*").reject { |d| d =~ %r{/archives$} }

  unless java?
    self.extra_deps += [
      # Keep this version in sync with the one in extconf.rb !
      ["mini_portile2",    "~> 2.0.0.rc2"],
    ]
  end

  self.extra_dev_deps += [
    ["hoe-bundler",     ">= 1.1"],
    ["hoe-debugging",   "~> 1.2.1"],
    ["hoe-gemspec",     ">= 1.0"],
    ["hoe-git",         ">= 1.4"],
    ["minitest",        "~> 2.2.2"],
    ["rake",            ">= 0.9"],
    ["rake-compiler",   "~> 0.9.2"],
    ["rake-compiler-dock", "~> 0.4.2"],
    ["racc",            ">= 1.4.6"],
    ["rexical",         ">= 1.0.5"]
  ]

  if java?
    self.spec_extras = { :platform => 'java' }
  else
    self.spec_extras = {
      :extensions => ["ext/leon/extconf.rb"],
      :required_ruby_version => '>= 1.9.2'
    }
  end

  self.testlib = :minitest
end

# ----------------------------------------

def add_file_to_gem relative_path
  target_path = File.join gem_build_path, relative_path
  target_dir = File.dirname(target_path)
  mkdir_p target_dir unless File.directory?(target_dir)
  rm_f target_path
  safe_ln relative_path, target_path
  HOE.spec.files += [relative_path]
end

def gem_build_path
  File.join 'pkg', HOE.spec.full_name
end

if java?
  # TODO: clean this section up.
  require "rake/javaextensiontask"
  Rake::JavaExtensionTask.new("leon", HOE.spec) do |ext|
    jruby_home = RbConfig::CONFIG['prefix']
    ext.ext_dir = 'ext/java'
    ext.lib_dir = 'lib/leon'
    ext.source_version = '1.6'
    ext.target_version = '1.6'
    jars = ["#{jruby_home}/lib/jruby.jar"] + FileList['lib/*.jar']
    ext.classpath = jars.map { |x| File.expand_path x }.join ':'
  end

  task gem_build_path => [:compile] do
    add_file_to_gem 'lib/leon/leon.jar'
  end
else
  begin
    require 'rake/extensioncompiler'
    # Ensure mingw compiler is installed
    Rake::ExtensionCompiler.mingw_host
    mingw_available = true
  rescue
    mingw_available = false
  end
  require "rake/extensiontask"

  HOE.spec.files.reject! { |f| f =~ %r{\.(java|jar)$} }

  dependencies = YAML.load_file("dependencies.yml")

  task gem_build_path do
    %w[libxml2 libxslt].each do |lib|
      version = dependencies[lib]
      archive = File.join("ports", "archives", "#{lib}-#{version}.tar.gz")
      add_file_to_gem archive
      patchesdir = File.join("patches", lib)
      patches = `#{['git', 'ls-files', patchesdir].shelljoin}`.split("\n").grep(/\.patch\z/)
      patches.each { |patch|
        add_file_to_gem patch
      }
      (untracked = Dir[File.join(patchesdir, '*.patch')] - patches).empty? or
        at_exit {
          untracked.each { |patch|
            puts "** WARNING: untracked patch file not added to gem: #{patch}"
          }
        }
    end
  end

  Rake::ExtensionTask.new("leon", HOE.spec) do |ext|
    ext.lib_dir = File.join(*['lib', 'leon', ENV['FAT_DIR']].compact)
    ext.config_options << ENV['EXTOPTS']
    if mingw_available
      ext.cross_compile  = true
      ext.cross_platform = CROSS_RUBIES.map(&:platform).uniq
      ext.cross_config_options << "--enable-cross-build"
      ext.cross_compiling do |spec|
        libs = dependencies.map { |name, version| "#{name}-#{version}" }.join(', ')

        spec.required_ruby_version = [
          '>= 1.9.2',
          "< #{CROSS_RUBIES.max_by(&:ver).minor_ver.succ}"
        ]

        spec.post_install_message = <<-EOS
Nokogiri is built with the packaged libraries: #{libs}.
        EOS
        spec.files.reject! { |path| File.fnmatch?('ports/*', path) }
      end
    end
  end
end

# ----------------------------------------

task 'gem:spec' => 'generate' if Rake::Task.task_defined?("gem:spec")

# This is a big hack to make sure that the racc and rexical
# dependencies in the Gemfile are constrainted to ruby platforms
# (i.e. MRI and Rubinius). There's no way to do that through hoe,
# and any solution will require changing hoe and hoe-bundler.
old_gemfile_task = Rake::Task['bundler:gemfile'] rescue nil
task 'bundler:gemfile' do
  old_gemfile_task.invoke if old_gemfile_task

  lines = File.open('Gemfile', 'r') { |f| f.readlines }.map do |line|
    line =~ /racc|rexical/ ? "#{line.strip}, :platform => :ruby" : line
  end
  File.open('Gemfile', 'w') { |f| lines.each { |line| f.puts line } }
end

# ----------------------------------------

desc "set environment variables to build and/or test with debug options"
task :debug do
  ENV['NOKOGIRI_DEBUG'] = "true"
  ENV['CFLAGS'] ||= ""
  ENV['CFLAGS'] += " -DDEBUG"
end

require 'tasks/test'

task :java_debug do
  ENV['JAVA_OPTS'] = '-Xdebug -Xrunjdwp:transport=dt_socket,address=8000,server=y,suspend=y' if java? && ENV['JAVA_DEBUG']
end

if java?
  task :test_19 => :test
  task :test_20 do
    ENV['JRUBY_OPTS'] = "--2.0"
    Rake::Task["test"].invoke
  end
end

Rake::Task[:test].prerequisites << :compile
Rake::Task[:test].prerequisites << :java_debug
Rake::Task[:test].prerequisites << :check_extra_deps unless java?

if Hoe.plugins.include?(:debugging)
  ['valgrind', 'valgrind:mem', 'valgrind:mem0'].each do |task_name|
    Rake::Task["test:#{task_name}"].prerequisites << :compile
  end
end

# ----------------------------------------

def verify_dll(dll, cross_ruby)
  dll_imports = cross_ruby.dlls
  dump = `#{['env', 'LANG=C', cross_ruby.tool('objdump'), '-p', dll].shelljoin}`
  raise "unexpected file format for generated dll #{dll}" unless /file format #{Regexp.quote(cross_ruby.target)}\s/ === dump
  raise "export function Init_leon not in dll #{dll}" unless /Table.*\sInit_leon\s/mi === dump

  # Verify that the expected DLL dependencies match the actual dependencies
  # and that no further dependencies exist.
  dll_imports_is = dump.scan(/DLL Name: (.*)$/).map(&:first).map(&:downcase).uniq
  if dll_imports_is.sort != dll_imports.sort
    raise "unexpected dll imports #{dll_imports_is.inspect} in #{dll}"
  end
  puts "#{dll}: Looks good!"
end

task :cross do
  rake_compiler_config_path = File.expand_path("~/.rake-compiler/config.yml")
  unless File.exists? rake_compiler_config_path
    raise "rake-compiler has not installed any cross rubies. Try using rake-compiler-dev-box for building binary windows gems.'"
  end

  CROSS_RUBIES.each do |cross_ruby|
    task "tmp/#{cross_ruby.platform}/leon/#{cross_ruby.ver}/leon.so" do |t|
      # To reduce the gem file size strip mingw32 dlls before packaging
      sh [cross_ruby.tool('strip'), '-S', t.name].shelljoin
      verify_dll t.name, cross_ruby
    end
  end
end

desc "build a windows gem without all the ceremony."
task "gem:windows" do
  require "rake_compiler_dock"
  RakeCompilerDock.sh "bundle && rake cross native gem MAKE='nice make -j`nproc`' RUBY_CC_VERSION=#{ENV['RUBY_CC_VERSION']}"
end

# vim: syntax=Ruby
