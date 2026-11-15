from scripts.check_gps import parse_nmea_sentence, sentence_fix


def test_parse_valid_nmea_and_detect_gga_fix():
    sentence = b"$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47"
    fields = parse_nmea_sentence(sentence)

    assert fields is not None
    assert sentence_fix(fields) == (48.1173, 11.516666666666667)


def test_reject_nmea_with_invalid_checksum():
    sentence = b"$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*00"

    assert parse_nmea_sentence(sentence) is None


def test_valid_gga_without_fix_has_no_position():
    body = b"GPGGA,123519,,,,,0,00,99.99,,,,,,"
    checksum = 0
    for byte in body:
        checksum ^= byte
    sentence = b"$" + body + b"*" + f"{checksum:02X}".encode()
    fields = parse_nmea_sentence(sentence)

    assert fields is not None
    assert sentence_fix(fields) is None
