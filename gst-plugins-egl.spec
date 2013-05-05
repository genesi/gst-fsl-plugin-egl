%define majorminor  0.10
%define gstreamer   gstreamer

%define gst_minver  0.10.0

Name: 		%{gstreamer}-plugins-egl
Version: 	0.10.2
Release: 	1.gst
Summary: 	GStreamer streaming media framework plug-ins

Group: 		Applications/Multimedia
License: 	LGPL
URL:		http://gstreamer.freedesktop.org/
Vendor:         GStreamer Backpackers Team <package@gstreamer.freedesktop.org>
Source:         http://gstreamer.freedesktop.org/src/gst-plugins-egl/gst-plugins-egl-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

Requires: 	  %{gstreamer} >= %{gst_minver}
BuildRequires: 	  %{gstreamer}-devel >= %{gst_minver}

BuildRequires:  gtk-doc >= 1.3

%description
GStreamer is a streaming media framework, based on graphs of filters which
operate on media data. Applications using this library can do anything
from real-time sound processing to playing videos, and just about anything
else media-related.  Its plugin-based architecture means that new data
types or processing capabilities can be added simply by installing new
plug-ins.

%prep
%setup -q -n gst-plugins-egl-%{version}
%build
%configure \
  --enable-gtk-doc

make %{?_smp_mflags}
                                                                                
%install
rm -rf $RPM_BUILD_ROOT

%makeinstall

# Clean out files that should not be part of the rpm.
rm -f $RPM_BUILD_ROOT%{_libdir}/gstreamer-%{majorminor}/*.la
rm -f $RPM_BUILD_ROOT%{_libdir}/gstreamer-%{majorminor}/*.a
rm -f $RPM_BUILD_ROOT%{_libdir}/*.a
rm -f $RPM_BUILD_ROOT%{_libdir}/*.la

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-, root, root)
%doc AUTHORS COPYING README
%{_libdir}/gstreamer-%{majorminor}/libgstopengl.so
%{_libdir}/libgstegl-%{majorminor}.so.*
%{_datadir}/locale/en/LC_MESSAGES/gst-plugins-egl-0.10.mo

%package devel
Summary:        GStreamer Plugin Library Headers
Group:          Development/Libraries
Requires:       %{gstreamer}-plugins-egl = %{version}

%description devel
GStreamer Plugins Base library development and header files.

%files devel
%defattr(-, root, root)
# plugin helper library headers
%{_includedir}/gstreamer-%{majorminor}/gst/gl/*.h
%{_libdir}/libgstegl-%{majorminor}.so
%{_libdir}/pkgconfig/gstreamer-gl-%{majorminor}.pc
%{_datadir}/gtk-doc

%changelog
* Mon Aug 17 2009 Christian Schaller <christian dot schaller at collabora.co.uk>
- First attempt at spec file
