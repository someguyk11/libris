"""ISBN edition identity. No network or database side effects."""
import re


def normalize_isbn(value: str) -> str:
    if not isinstance(value, str) or len(value) > 64:
        raise ValueError('Enter an ISBN-10 or ISBN-13, optionally using spaces and hyphens.')
    compact = re.sub(r'[\s-]', '', value).upper()
    if re.fullmatch(r'[0-9]{9}[0-9X]', compact):
        digits = [int(c) if c != 'X' else 10 for c in compact]
        valid = sum((10 - i) * digit for i, digit in enumerate(digits)) % 11 == 0
    elif re.fullmatch(r'97[89][0-9]{10}', compact):
        valid = sum(int(c) * (1 if i % 2 == 0 else 3) for i, c in enumerate(compact)) % 10 == 0
    else:
        raise ValueError('Use 10 ISBN characters or 13 digits beginning with 978 or 979.')
    if not valid:
        raise ValueError('The ISBN checksum is incorrect. Check the number and try again.')
    return compact


def isbn13(value: str) -> str:
    compact = normalize_isbn(value)
    if len(compact) == 13:
        return compact
    body = '978' + compact[:9]
    return body + str((-sum(int(c) * (1 if i % 2 == 0 else 3) for i, c in enumerate(body))) % 10)


def isbn10(value: str) -> str | None:
    canonical = isbn13(value)
    if not canonical.startswith('978'):
        return None
    body = canonical[3:12]
    check = (-sum((10 - i) * int(c) for i, c in enumerate(body))) % 11
    return body + ('X' if check == 10 else str(check))
