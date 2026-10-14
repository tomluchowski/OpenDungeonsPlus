"""Run source/tests/test_LevelScript.cpp without Boost.Test.

The test file is compiled together with LevelScript.cpp against a tiny stand-in for the
few Boost.Test macros it uses, so the parser, the writer and the region handling of the
level script are checked on machines that have no Boost.Test.
"""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import time

repo = Path(__file__).resolve().parents[2]

shim = r'''
#include <iostream>
#include <string>
#include <vector>

struct ShimCase {std::string name; void (*body)();};
inline std::vector<ShimCase>& shimCases() {static std::vector<ShimCase> cases; return cases;}
inline int& shimFailures() {static int failures = 0; return failures;}
struct ShimRegister
{
    ShimRegister(const std::string& name, void (*body)())
    { ShimCase entry; entry.name = name; entry.body = body; shimCases().push_back(entry); }
};
struct ShimAbort {};

#define BOOST_AUTO_TEST_CASE(name) \
    static void name(); \
    static ShimRegister shimRegister_##name(#name, name); \
    static void name()
#define SHIM_CHECK(cond, text, fatal) \
    do { if(!(cond)) { ++shimFailures(); std::cout << "FAIL " << __LINE__ << ": " << text << "\n"; \
         if(fatal) throw ShimAbort(); } } while(0)
#define BOOST_CHECK(cond) SHIM_CHECK((cond), #cond, false)
#define BOOST_REQUIRE(cond) SHIM_CHECK((cond), #cond, true)
#define BOOST_CHECK_EQUAL(a, b) SHIM_CHECK((a) == (b), #a " == " #b, false)
#define BOOST_REQUIRE_EQUAL(a, b) SHIM_CHECK((a) == (b), #a " == " #b, true)
#define BOOST_CHECK_MESSAGE(cond, msg) SHIM_CHECK((cond), msg, false)
'''

main = r'''
#include "BoostTestTargetConfig.h"
int main()
{
    for(std::size_t i = 0; i < shimCases().size(); ++i)
    {
        try { shimCases()[i].body(); }
        catch(const ShimAbort&) {}
    }
    std::cout << shimCases().size() << " cases, " << shimFailures() << " failures\n";
    return shimFailures() == 0 ? 0 : 1;
}
'''


def find_vcvars():
    """Returns the path of vcvars64.bat of an installed Visual Studio 2022, or None."""
    candidates = [
        os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)'),
        os.environ.get('ProgramFiles', r'C:\Program Files'),
    ]
    for base in candidates:
        for edition in ('BuildTools', 'Community', 'Professional', 'Enterprise'):
            path = Path(base) / 'Microsoft Visual Studio' / '2022' / edition / 'VC/Auxiliary/Build/vcvars64.bat'
            if path.exists():
                return path
    return None


with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    (tmp / 'BoostTestTargetConfig.h').write_text(shim)
    (tmp / 'main.cpp').write_text(main)
    sources = [repo / 'source/gamemap/LevelScript.cpp', repo / 'source/tests/test_LevelScript.cpp', tmp / 'main.cpp']
    include = ['-I' + str(tmp), '-I' + str(repo / 'source')]
    exe = tmp / 'level_script_check.exe'

    compiler = shutil.which('g++') or shutil.which('clang++')
    if compiler:
        command = [compiler, '-std=c++11', '-o', str(exe)] + include + [str(s) for s in sources]
        subprocess.run(command, check=True)
    else:
        vcvars = find_vcvars()
        if shutil.which('cl') is None and vcvars is None:
            print('SKIP: no C++ compiler found')
            sys.exit(0)
        prefix = '' if shutil.which('cl') else 'call "%s" >nul && ' % vcvars
        flags = '/nologo /EHsc /std:c++14 /Fe"%s" /Fo"%s/" /I"%s" /I"%s"' % (
            exe, tmp.as_posix(), tmp, repo / 'source')
        line = prefix + 'cl ' + flags + ' ' + ' '.join('"%s"' % s for s in sources)
        subprocess.run(line, shell=True, check=True)

    # Windows application control sometimes blocks a freshly built program (WinError 4551),
    # which goes away when the start is repeated
    for attempt in range(8):
        try:
            result = subprocess.run([str(exe)])
            break
        except OSError:
            if attempt == 7:
                raise
            time.sleep(2)
    sys.exit(result.returncode)
