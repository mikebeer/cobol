%global _build_id_links none
Name:           cobol
Version:        1.0
Release:        1
Summary:        COBOL to C89 translator
License:        Unspecified (set by the project owner)
URL:            https://github.com/mikebeer/cobol
Source0:        cobol-%{version}.tar.gz
BuildRequires:  gcc, make

%description
cobol translates COBOL source to portable C89. The generated program
includes cobrt.h (installed in /usr/share/cobol) and is compiled with
any C compiler; "cobol -c prog.cob -o prog" does both steps.

%prep
%setup -q

%build
make %{?_smp_mflags} CFLAGS="%{optflags} -std=gnu89" LIBS="-lm"

%install
make install DESTDIR=%{buildroot} PREFIX=/usr

%files
/usr/bin/cobol
/usr/share/cobol

%changelog
* Mon Oct 12 2026 cobol project - 1.0-1
- First release
