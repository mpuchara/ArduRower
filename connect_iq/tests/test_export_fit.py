"""Round-trip ArduRower developer and native fields through the official FIT SDK."""
import unittest

from garmin_fit_sdk import Decoder, Encoder, Stream

from export_fit import APP_ID, MESSAGE, ROWER_FIELDS, ROWER_TYPES, export_fit


def fixture(reset=False, tail=0, sideload=False, missing_definition=False):
    start = 1_150_000_000
    encoder = Encoder()
    encoder.on_mesg(MESSAGE["FILE_ID"], {"type": "activity", "time_created": start})
    developer = {"developer_data_index": 0,
                 "application_id": [0] * 16 if sideload else list(APP_ID)}
    encoder.on_mesg(MESSAGE["DEVELOPER_DATA_ID"], developer)
    for key, kind in ROWER_TYPES.items():
        if missing_definition and key == 9:
            continue
        name, units = ROWER_FIELDS[key]
        description = {"developer_data_index": 0, "field_definition_number": key,
                       "fit_base_type_id": kind, "field_name": name, "units": units}
        encoder.add_developer_field(key, developer, description)
        encoder.on_mesg(MESSAGE["FIELD_DESCRIPTION"], description)
    encoder.on_mesg(MESSAGE["RECORD"], {
        "timestamp": start, "distance": 0, "heart_rate": 145, "power": 201,
        "developer_fields": {0: 0, 1: 4, 2: 22.5, 3: 140, 4: 1, 5: 0, 6: 0xffff},
    })
    encoder.on_mesg(MESSAGE["RECORD"], {
        "timestamp": start+30, "distance": 0, "heart_rate": 146,
        "developer_fields": {0: 100, 1: 3.5, 2: 24.5, 3: 145, 4: 1, 5: 10,
                             6: 28, 7: 140, 8: 23.5, 10: 129},
    })
    if reset:
        encoder.on_mesg(MESSAGE["RECORD"], {
            "timestamp": start+31, "developer_fields": {0: 1, 5: 0},
        })
    summary = {
        "start_time": start, "timestamp": start+32+tail, "sport": "rowing", "sub_sport": "indoor_rowing",
        "total_distance": 0, "total_timer_time": 32, "avg_heart_rate": 146,
    }
    if sideload:
        summary.update(total_elapsed_time=32, enhanced_avg_speed=0)
        encoder.on_mesg(MESSAGE["LAP"], summary)
    encoder.on_mesg(MESSAGE["SESSION"], summary)
    return bytes(encoder.close())


class FitExportTest(unittest.TestCase):
    def test_options_and_native_data(self):
        for replace in (False, True):
            output, updates = export_fit(fixture(), replace)
            self.assertEqual(updates["power"], 1)
            self.assertEqual(updates["distance"], int(replace))
            self.assertTrue(Decoder(Stream.from_byte_array(output)).check_integrity())
            messages, errors = Decoder(Stream.from_byte_array(output)).read()
            self.assertFalse(errors)
            first, second = messages["record_mesgs"]
            self.assertEqual(first["power"], 201)  # Keep a valid native sensor value.
            self.assertEqual(second["heart_rate"], 146)
            self.assertEqual(second["power"], 145)
            self.assertEqual(second["speed"], 3.5)
            self.assertEqual(second["cadence"], 24)
            self.assertEqual(second["fractional_cadence"], 0.5)
            self.assertEqual(second["distance"], 100 if replace else 0)
            self.assertIn(145, second["developer_fields"].values())
            summary = messages["session_mesgs"][0]
            self.assertEqual(summary["total_distance"], 100 if replace else 0)
            self.assertEqual(summary["total_cycles"], 10)
            self.assertEqual(summary["avg_power"], 129)
            self.assertEqual(summary["avg_heart_rate"], 146)
            self.assertEqual(summary["total_timer_time"], 32)
            self.assertAlmostEqual(summary["avg_speed"], 500 / 140, places=3)

    def test_incomplete_summary(self):
        _, updates = export_fit(fixture(tail=60), True)
        self.assertEqual(updates["distance"], 1)
        self.assertNotIn("total_distance", updates)
        self.assertNotIn("avg_power", updates)

    def test_sideload_zero_id_invalid_integers_and_delayed_save(self):
        data, updates = export_fit(fixture(sideload=True, tail=30), True)
        self.assertEqual(updates["total_distance"], 2)
        messages, errors = Decoder(Stream.from_byte_array(data)).read()
        self.assertFalse(errors)
        for kind in ("session_mesgs", "lap_mesgs"):
            summary = messages[kind][0]
            self.assertEqual(summary["total_distance"], 100)
            self.assertEqual(summary["avg_power"], 129)
            self.assertAlmostEqual(summary["enhanced_avg_speed"], 500 / 140, places=3)
        with self.assertRaisesRegex(ValueError, "No valid ArduRower"):
            export_fit(fixture(sideload=True, missing_definition=True))

    def test_input_integrity_and_reset(self):
        with self.assertRaisesRegex(ValueError, "integrity"):
            export_fit(fixture()[:-2])
        with self.assertRaisesRegex(ValueError, "counters decreased"):
            export_fit(fixture(reset=True))


if __name__ == "__main__":
    unittest.main()
