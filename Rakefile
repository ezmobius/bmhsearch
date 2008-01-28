require 'rake'
require 'rake/testtask'
require 'rake/clean'
require 'rake/gempackagetask'
require 'rake/rdoctask'
require 'tools/rakehelp'
require 'fileutils'
include FileUtils

setup_tests
setup_clean ["ext/bmh_search/*.{bundle,so,obj,pdb,lib,def,exp}", "ext/bmh_search/Makefile", "pkg", "lib/*.bundle", "*.gem", ".config"]

setup_rdoc ['README', 'lib/**/*.rb', 'doc/**/*.rdoc', 'ext/bmh_search/bmh_search.c']

desc "Does a full compile, test run"
task :default => [:compile, :test]

desc "Compiles all extensions"
task :compile => [:bmh_search] do
  if Dir.glob(File.join("lib","bmh_search.*")).length == 0
    STDERR.puts "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
    STDERR.puts "Gem actually failed to build.  Your system is"
    STDERR.puts "NOT configured properly to build Mongrel."
    STDERR.puts "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
    exit(1)
  end
end

task :package => [:clean,:compile,:test]


setup_extension("bmh_search", "bmh_search")

name="bmh_search"
version="0.0.1"

setup_gem(name, version) do |spec|
  spec.summary = "BMHSearch for fast multipart mime boundary parsing."
  spec.description = spec.summary
  spec.test_files = Dir.glob('test/test_*.rb')
  spec.author="Zed A. Shaw"
  spec.executables=[]
  spec.files += %w(README Rakefile)

  spec.required_ruby_version = '>= 1.8.4'
end

task :install do
  sh %{rake package}
  sh %{gem install pkg/bmh_search-#{version}}
end

task :uninstall => [:clean] do
  sh %{gem uninstall bmh_search}
end


task :gem_source do
  mkdir_p "pkg/gems"
 
  FileList["**/*.gem"].each { |gem| mv gem, "pkg/gems" }
  FileList["pkg/*.tgz"].each {|tgz| rm tgz }
  rm_rf "pkg/#{name}-#{version}"

  sh %{ index_gem_repository.rb -d pkg }
  sh %{ scp -r ChangeLog pkg/* #{ENV['SSH_USER']}@rubyforge.org:/var/www/gforge-projects/mongrel/releases/ }
end
