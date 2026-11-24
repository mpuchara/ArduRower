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
    8: ("Rower average cadence", "spm"), 9: ("Rower 500m pace", "s/500m"),
    10: ("Rower avg equiv power", "W"),
}
ROWER_TYPES = {0: 0x88, 1: 0x88, 2: 0x88, 3: 0x88, 4: 2, 5: 0x84,
               6: 0x84, 7: 0x84, 8: 0x88, 9: 0x84, 10: 0x84}


def rower_field_keys(descriptions):
    recognized = {}
    anonymous = {}
    for key, developer, description in descriptions:
        field_id = description.get("field_definition_number")
        signature = description.get("field_name"), description.get("units") or ""
        if (ROWER_FIELDS.get(field_id) != signature or
                ROWER_TYPES.get(field_id) != description.get("fit_base_type_id")):
            continue
        app_id = developer.get("application_id")
        if app_id == list(APP_ID):
            recognized[key] = field_id
        elif app_id == [0] * len(APP_ID):
            anonymous.setdefault(description["developer_data_index"], {})[key] = field_id
    if recognized:
        return recognized
    # The USB-installed field on fēnix 6X can write an all-zero application ID.
    # Accept only a single developer with the complete typed 0.1.7 schema.
    complete = [fields for fields in anonymous.values()
                if len(fields) == len(ROWER_FIELDS) and set(fields.values()) == set(ROWER_FIELDS)]
    return complete[0] if len(complete) == 1 else {}


def rower_values(record, keys):
    return {keys[key]: value for key, value in record.get("developer_fields", {}).items()
            if key in keys and isinstance(value, (int, float)) and math.isfinite(value)
            and value >= 0 and not (ROWER_TYPES[keys[key]] == 0x84 and value == 0xffff)}


def export_fit(source, replace_zero_distance=False):
    if not Decoder(Stream.from_byte_array(source)).check_integrity():
        raise ValueError("Input failed the FIT CRC/integrity check")
    decoder = Decoder(Stream.from_byte_array(source))

    messages = []
    descriptions = []
    encoder = Encoder()

    def register(key, developer, description):
        encoder.add_developer_field(key, developer, description)
        descriptions.append((key, developer, description))

    _, errors = decoder.read(
        convert_datetimes_to_dates=False, expand_components=False,
        expand_sub_fields=False, merge_heart_rates=False,
        mesg_listener=lambda number, message: messages.append((number, message)),
        field_description_listener=register,
    )
    if errors:
        raise ValueError(f"Input could not be decoded: {errors[0]}")
    rower_keys = rower_field_keys(descriptions)
    sessions = [msg for number, msg in messages if number == MESSAGE["SESSION"]]
    if len(sessions) != 1 or (
        sessions[0].get("sport") != "rowing" and sessions[0].get("sub_sport") != "indoor_rowing"
    ):
        raise ValueError("Expected a single rowing session")
    session = sessions[0]
    start, end = session.get("start_time"), session.get("timestamp")
    if start is None or end is None or start > end:
        raise ValueError("Session timestamps are missing or invalid")
    elapsed = session.get("total_elapsed_time")
    if isinstance(elapsed, (int, float)) and math.isfinite(elapsed) and elapsed > 0:
        # Some watches use save time as the summary timestamp.
        end = min(end, start + math.ceil(elapsed))

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
        fields = rower_values(message, rower_keys)
        if not fields or fields.get(4, 1) != 1:
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
        laps = [msg for number, msg in messages if number == MESSAGE["LAP"]]
        summaries = [session]
        if len(laps) == 1 and laps[0].get("start_time") == start:
            summaries.append(laps[0])
        for summary in summaries:
            zero_distance = (replace_zero_distance and summary.get("total_distance") == 0
                             and last.get(0, 0) > 0)
            add(summary, "total_distance", last.get(0), replace_zero_distance)
            add(summary, "total_cycles", last.get(5))
            if last.get(7, 0):
                speed = 500 / last[7]
                if summary.get("enhanced_avg_speed") is None:
                    add(summary, "avg_speed", speed, zero_distance)
                elif zero_distance and not summary.get("avg_speed"):
                    add(summary, "enhanced_avg_speed", speed, True)
            add_cadence(summary, "avg_cadence", "avg_fractional_cadence", last.get(8))
            add(summary, "avg_power", last.get(10))

    for number, message in messages:
        if number not in Profile["messages"]:
            updates["unsupported_messages_skipped"] += 1
            continue
        if "developer_fields" in message:
            # FIT float32 missing values decode to NaN. Omit them so the
            # SDK can write the copy without treating them as samples.
            message["developer_fields"] = {
                key: value for key, value in message["developer_fields"].items()
                if not (isinstance(value, float) and not math.isfinite(value))
            }
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
