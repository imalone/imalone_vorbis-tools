%define name	vorbis-tools
%define version	1.0rc2
%define release 1

Summary:	Several Ogg Vorbis Tools
Name:		%{name}
Version:	%{version}
Release:	%{release}
Group:		Libraries/Multimedia
Copyright:	GPL
URL:		http://www.xiph.org/
Vendor:		Xiphophorus <team@xiph.org>
Source:		ftp://ftp.xiph.org/pub/vorbis-tools/%{name}-%{version}.tar.gz
BuildRoot:	%{_tmppath}/%{name}-root
Requires:       libogg >= 1.0rc2
Requires:       libvorbis >= 1.0rc2
Requires:       libao >= 0.8.0
Prefix:		%{_prefix}

%description
vorbis-tools contains oggenc (and encoder) and ogg123 (a playback tool)

%prep
%setup -q -n %{name}-%{version}

%build
if [ ! -f configure ]; then
  CFLAGS="$RPM_OPT_FLAGS" ./autogen.sh --prefix=%{_prefix}
else
  CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{_prefix}
fi
make

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install

%files
%defattr(-,root,root)
%doc COPYING
%doc README
%doc ogg123/ogg123rc-example
%{_bindir}/oggenc
%{_bindir}/ogg123
%{_bindir}/ogginfo
%{_bindir}/vorbiscomment
%{_datadir}/man/man1/ogg123.1*
%{_datadir}/man/man1/oggenc.1*
%{_datadir}/man/man1/ogginfo.1*

%clean 
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%post

%postun

%changelog
* Sun Oct 07 2001 Jack Moffitt <jack@xiph.org>
- Updated for configurable prefix
* Sun Aug 12 2001 Greg Maxwell <greg@linuxpower.cx>
- updated for rc2
* Sun Jun 17 2001 Jack Moffitt <jack@icecast.org>
- updated for rc1
- added ogginfo
* Mon Jan 22 2001 Jack Moffitt <jack@icecast.org>
- updated for prebeta4 builds
* Sun Oct 29 2000 Jack Moffitt <jack@icecast.org>
- initial spec file created
