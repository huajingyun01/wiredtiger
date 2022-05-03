#!/usr/bin/env python
#
# Public Domain 2014-present MongoDB, Inc.
# Public Domain 2008-2014 WiredTiger, Inc.
#
# This is free and unencumbered software released into the public domain.
#
# Anyone is free to copy, modify, publish, use, compile, sell, or
# distribute this software, either in source code form or as a compiled
# binary, for any purpose, commercial or non-commercial, and by any
# means.
#
# In jurisdictions that recognize copyright laws, the author or authors
# of this software dedicate any and all copyright interest in the
# software to the public domain. We make this dedication for the benefit
# of the public at large and to the detriment of our heirs and
# successors. We intend this dedication to be an overt act of
# relinquishment in perpetuity of all present and future rights to this
# software under copyright law.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#
# test_tiered16.py
#    Basic test for the remove_shared configuration in session.drop.

from helper_tiered import TieredConfigMixin, tiered_storage_sources
from wtscenario import make_scenarios
import os, wiredtiger, wttest

class test_tiered16(TieredConfigMixin, wttest.WiredTigerTestCase):
    scenarios = make_scenarios(tiered_storage_sources)

    def test_remove_shared(self):
        uri_a = "table:tiereda"
        uri_b = "table:tieredb"

        self.session.create(uri_a, "key_format=S,value_format=S")
        self.session.create(uri_b, "key_format=S,value_format=S")

        # It is invalid for the user to attempt to force removal of shared files
        # if they have configured for underlying files to not be removed.
        self.assertRaises(wiredtiger.WiredTigerError,
            lambda: self.session.drop(uri_a, "remove_files=false,remove_shared=true"))
        self.assertRaises(wiredtiger.WiredTigerError,
            lambda: self.session.drop(uri_a, "force=true,remove_files=false,remove_shared=true"))

        c = self.session.open_cursor(uri_a)
        c["a"] = "a"
        c["b"] = "b"
        c.close()
        self.session.checkpoint()
        self.session.flush_tier(None)

        c2 = self.session.open_cursor(uri_b)
        c2["c"] = "c"
        c2["d"] = "d"
        c2.close()
        self.session.checkpoint()
        self.session.flush_tier(None)

        self.session.drop(uri_a, "remove_files=true,remove_shared=true")

        # The shared object files corresponding to the first table should have been removed from
        # both the bucket and cache directories but the shared object files corresponding to the
        # second table should still remain.
        if self.is_tiered_scenario():
            self.assertFalse(os.path.isfile("bucket1/pfx_tiereda-0000000001.wtobj"))
            self.assertFalse(os.path.isfile("bucket1/pfx_tiereda-0000000002.wtobj"))
            self.assertTrue(os.path.isfile("bucket1/pfx_tieredb-0000000001.wtobj"))
            self.assertTrue(os.path.isfile("bucket1/pfx_tieredb-0000000002.wtobj"))

            self.assertFalse(os.path.isfile("cache-bucket1/pfx_tiereda-0000000001.wtobj"))
            self.assertFalse(os.path.isfile("cache-bucket1/pfx_tiereda-0000000002.wtobj"))
            self.assertTrue(os.path.isfile("cache-bucket1/pfx_tieredb-0000000001.wtobj"))
            self.assertTrue(os.path.isfile("cache-bucket1/pfx_tieredb-0000000002.wtobj"))

        self.session.drop(uri_b, "remove_files=true,remove_shared=true")

        # The shared object files corresponding to the second table should have been removed.
        if self.is_tiered_scenario():
            self.assertFalse(os.path.isfile("bucket1/pfx_tieredb-0000000001.wtobj"))
            self.assertFalse(os.path.isfile("bucket1/pfx_tieredb-0000000002.wtobj"))

            self.assertFalse(os.path.isfile("cache-bucket1/pfx_tieredb-0000000001.wtobj"))
            self.assertFalse(os.path.isfile("cache-bucket1/pfx_tieredb-0000000002.wtobj"))

if __name__ == '__main__':
    wttest.run
