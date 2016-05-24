Gem::Specification.new do |g|
  g.name = 'leon'
  g.version = '0.0.1'
  g.date = '2015-12-21'
  g.summary = 'LEON serialization for Ruby.'
  g.description = 'Serialize data to a buffer.'
  g.authors = ["Raymond Pulver IV"]
  g.email = "raymond.pulver_iv@uconn.edu"
  g.files = Dir.glob("ext/leon/*.{c,h}")
  g.extensions << "ext/leon/extconf.rb"
  g.license = "MIT"
end
