Name:           pid-view
Version:        0.5
Release:        alt1
Summary:        Simple utility for viewing process information


License:        GPL-3.0-or-later
URL:            https://github.com/rimkamix/PID-View

Source0:        pid-view

BuildArch:      x86_64

%description
PID-View is a simple utility for viewing information about Linux processes.

%install
install -Dm755 %{SOURCE0} %{buildroot}%{_bindir}/pid-view

%files
%{_bindir}/pid-view

%changelog
* Thu Jun 25 2026 Zhenya Borodin <rimkamix0@gmail.com> 0.5-alt1
- Initial package
