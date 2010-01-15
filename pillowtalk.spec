Name: pillowtalk
Version: 0.3
Summary: ANSI C library that talks to CouchDB using libcurl and yajl
Release: 2
Source: %{name}-%{version}.tgz

License: MIT-LICENSE
Group: Development/Tools
URL: http://github.com/jubos/pillowtalk/
Packager: Guy Albertelli (guy@kosmix.com)
BuildRoot: /var/tmp/%{name}-%{version}.root

%description
ANSI C library that talks to CouchDB using libcurl and yajl


%prep
%setup -n %{name}-%{version}

%build
pwd
./configure --prefix=$RPM_BUILD_ROOT
cmake -DCMAKE_INSTALL_PREFIX=$RPM_BUILD_ROOT/usr
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT%{_bindir}
mkdir -p $RPM_BUILD_ROOT%{_docdir}/%{name}-%{version}
make install
#cp -R *.html *.png *.css LICENSE*.txt images jam $RPM_BUILD_ROOT%{_docdir}/%{name}-%{version}

%files
%defattr(-,root,root)
%attr(644,root,root) /usr/lib/*
%attr(644,root,root) /usr/include/*



%clean
rm -rf $RPM_BUILD_ROOT
