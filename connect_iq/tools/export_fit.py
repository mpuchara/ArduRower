#!/usr/bin/env python3
"""Export a copy of an indoor rowing FIT with selected ArduRower native fields."""

import argparse
from collections import Counter
import math
from pathlib import Path

from garmin_fit_sdk import Decoder, Encoder, Profile, Stream


APP_ID = bytes.fromhex("a681ec0b07524e0dadd096027ee291b1")
MESSAGE = Profile["mesg_num"]
ROWER_FIELDS = {
    0: ("Rower distance", "m"), 1: ("Rower speed", "m/s"),
    2: ("Rower cadence", "spm"), 3: ("Rower equiv power", "W"),
    4: ("Rower BLE valid", ""), 5: ("Rower strokes", "strokes"),
    6: ("Rower active time", "s"), 7: ("Rower average 500m pace", "s/500m"),
    8: ("Rower average cadence", "spm"), 10: ("Rower avg equiv power", "W"),
}


def export_fit(source, replace_zero_distance=False):
    if not Decoder(Stream.from_byte_array(source)).check_integrity():
        raise ValueError("Input failed the FIT CRC/integrity check")
    decoder = Decoder(Stream.from_byte_array(source))

    messages = []
    rower_keys = {}
    encoder = Encoder()

    def register(key, developer, description):
        encoder.add_developer_field(key, developer, description)
        field_id = description.get("field_definition_number")
        if (developer.get("application_id") == list(APP_ID) and
                ROWER_FIELDS.get(field_id) ==
                (description.get("field_name"), description.get("units") or "")):
            rower_keys[key] = field_id

    _, errors = decoder.read(
        convert_datetimes_to_dates=False, expand_components=False,
        expand_sub_fields=False, merge_heart_rates=False,
        mesg_listener=lambda number, message: messages.append((number, message)),
        field_description_listener=register,
    )
    if errors:
        raise ValueError(f"Input could not be decoded: {errors[0]}")
    sessions = [msg for number, msg in messages if number == MESSAGE["SESSION"]]
    if len(sessions) != 1 or (
        sessions[0].get("sport") != "rowing" and sessions[0].get("sub_sport") != "indoor_rowing"
    ):
        raise ValueError("Expected a single rowing session")
    session = sessions[0]
    start, end = session.get("start_time"), session.get("timestamp")
    if start is None or end is None or start > end:
        raise ValueError("Session timestamps are missing or invalid")

    updates = Counter()

    def add(message, field, value, replace_zero=False):
        if value is not None and (message.get(field) is None or
                                  replace_zero and message[field] == 0 and value > 0):
            message[field] = value
            updates[field] += 1

    def add_cadence(message, field, fractional_field, value):
        if value is None or not 0 <= value < 255 or message.get(field) is not None:
            return
        add(message, field, int(value))
        if value % 1:
            add(message, fractional_field, value % 1)

    previous = {}
    last = None
    last_time = None
    for number, message in messages:
        timestamp = message.get("timestamp")
        if number != MESSAGE["RECORD"] or timestamp is None or not start <= timestamp <= end:
            continue
        fields = {rower_keys[key]: value for key, value in
                  message.get("developer_fields", {}).items()
                  if key in rower_keys and isinstance(value, (int, float))
                  and math.isfinite(value) and value >= 0}
        if not fields or fields.get(4) == 0:
            continue
        for counter in (0, 5, 6):
            if counter in fields:
                if fields[counter] < previous.get(counter, 0):
                    raise ValueError("ArduRower counters decreased. Export one ergometer session at a time.")
                previous[counter] = fields[counter]
        add(message, "distance", fields.get(0), replace_zero_distance)
        if message.get("enhanced_speed") is None:
            add(message, "speed", fields.get(1))
        add_cadence(message, "cadence", "fractional_cadence", fields.get(2))
        add(message, "power", fields.get(3))
        add(message, "total_cycles", fields.get(5))
        last, last_time = fields, timestamp
    if last is None:
        raise ValueError("No valid ArduRower developer samples were found")

    # A missing tail cannot establish the full workout totals. Keep the
    # watch's timer and HR summaries even when the ergometer supplies time.
    if end - last_time <= 10:
        add(session, "total_distance", last.get(0), replace_zero_distance)
        add(session, "total_cycles", last.get(5))
        if last.get(7, 0) and session.get("enhanced_avg_speed") is None:
            add(session, "avg_speed", 500 / last[7])
        add_cadence(session, "avg_cadence", "avg_fractional_cadence", last.get(8))
        add(session, "avg_power", last.get(10))

    for number, message in messages:
        encoder.on_mesg(number, message)
    output = bytes(encoder.close())
    if not Decoder(Stream.from_byte_array(output)).check_integrity():
        raise ValueError("Output failed the FIT integrity check")
    return output, updates


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path, help="new .fit file; original is never overwritten")
    parser.add_argument("--replace-zero-distance", action="store_true",
                        help="replace native zero distance when a positive ArduRower value exists")
    args = parser.parse_args()
    try:
        output, updates = export_fit(args.source.read_bytes(), args.replace_zero_distance)
        with args.output.open("xb") as file:
            file.write(output)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(f"{args.output}: {dict(updates)}")


if __name__ == "__main__":
    main()
