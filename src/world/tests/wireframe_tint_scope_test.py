"""Keep owner tint lookups confined to the three informational silhouettes."""
from pathlib import Path
import re
import unittest


class WireframeTintScopeTest(unittest.TestCase):
    def test_owner_tints_are_used_only_by_information_silhouettes(self):
        source = (Path(__file__).resolve().parents[2] / 'yw_game_ui.cpp').read_text()
        allowed = ('yw_RenderInfoVehicleWire', 'yw_RenderInfoWeaponWire', 'yw_RenderHUDInfo')
        # Find complete top-level function bodies; nested blocks are indented.
        functions = re.findall(r'(?ms)^([^\n;{}]+\([^;{}]*\))\s*\n\{(.*?)^\}', source)
        uses = [(signature, body) for signature, body in functions
                if 'GetOwnerWireframeTint(' in body]
        self.assertEqual(source.count('GetOwnerWireframeTint('), 3)
        self.assertEqual(len(uses), 3)
        for signature, body in uses:
            self.assertTrue(any(name + '(' in signature for name in allowed), signature)
        self.assertNotIn('yw_GemTargetNameColor', source)


if __name__ == '__main__':
    unittest.main()
