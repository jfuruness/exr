import csv

import python_example


def get_announcements_from_tsv(path: str) -> list[python_example.Announcement]:
    announcements = []

    with open(path, 'r', newline='') as file:
        reader = csv.reader(file, delimiter='\t')
        headers = next(reader)

        expected_header_start = "prefix\tas_path\ttimestamp\tseed_asn\troa_valid_length\troa_origin\trecv_relationship\twithdraw\ttraceback_end\tcommunities"
        if '\t'.join(headers)[:len(expected_header_start)] != expected_header_start:
            raise RuntimeError("TSV file header does not start with the expected format.")

        for row in reader:
            prefix = row[0]
            as_path = [int(asn) for asn in row[1].strip('{}').split(',')] if row[1] else []
            timestamp = int(row[2])
            seed_asn = int(row[3]) if row[3] else None
            roa_valid_length = row[4] == 'True' if row[4] else None
            roa_origin = int(row[5]) if row[5] else None

            relationship_mapping = {
                '0': python_example.Relationships.ORIGIN,
                '1': python_example.Relationships.PROVIDERS,
                '2': python_example.Relationships.PEERS,
                '3': python_example.Relationships.CUSTOMERS,
            }
            recv_relationship = relationship_mapping.get(row[6])
            if recv_relationship is None:
                raise RuntimeError("Invalid recv_relationship value: " + row[6])

            withdraw = row[7] == 'True'
            traceback_end = row[8] == 'True'
            communities = row[9].strip('[]').split(',') if row[9] else []

            announcement = python_example.Announcement(prefix, as_path, timestamp, seed_asn, roa_valid_length,
                                        roa_origin, recv_relationship, withdraw, traceback_end,
                                        communities)
            announcements.append(announcement)

    return announcements

# python_example.main()
# print("completed main")
engine = python_example.get_engine()
# engine.set_as_classes()
# ann = python_example.Announcement("1.2.", [1], 1, 1, None, None, python_example.Relationships.ORIGIN, False, True, [])
# anns = [ann]
anns = get_announcements_from_tsv("/home/anon/Desktop/anns_1000_mod.tsv")
engine.setup(anns)
engine.run(0)


