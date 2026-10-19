Name:       password-store

Summary:    Client for pass, the standard unix password manager
Version:    0.2.4
Release:    1
# The app is GPLv3, the bundled rnp BSD-2-Clause
License:    GPLv3 and BSD
URL:        https://github.com/klahr/password-store
Source0:    %{name}-%{version}.tar.bz2
Requires:   sailfishsilica-qt5 >= 0.10.9
Requires:   nemo-qml-plugin-configuration-qt5
Requires:   git
Requires:   openssh-clients
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  desktop-file-utils
BuildRequires:  cmake
BuildRequires:  pkgconfig(openssl)
BuildRequires:  pkgconfig(json-c)
BuildRequires:  pkgconfig(zlib)
BuildRequires:  bzip2-devel

%description
Browse, decrypt, edit and generate passwords in a pass password store,
with one-time codes and git sync. Keys are kept in an app-private keyring.


%prep
%setup -q -n %{name}-%{version}

%build

%qmake5

%make_build


%install
%qmake5_install


desktop-file-install --delete-original         --dir %{buildroot}%{_datadir}/applications                %{buildroot}%{_datadir}/applications/*.desktop

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_sysconfdir}/sailjail/permissions/%{name}.profile
%{_datadir}/icons/hicolor/*/apps/%{name}.png
