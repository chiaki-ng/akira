#!/usr/bin/env python3
import argparse
import json
import string
import sys
import tomllib
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parent.parent
BOREALIS_CATALOGS = REPOSITORY / "resources" / "i18n"
ARISA_CATALOGS = REPOSITORY / "resources" / "strings"
AKIRA_CATALOG_FILES = ("akira.json",)
DEFAULT_LOCALE = "en-US"
PLURAL_FORMS = ("zero", "one", "two", "few", "many")
ARISA_LOCALE_IDENTIFIERS = {
    "en-US": "en",
    "en-GB": "en-GB",
    "ja": "ja",
    "fr": "fr",
    "fr-CA": "fr-CA",
    "de": "de",
    "it": "it",
    "es": "es",
    "es-419": "es-419",
    "zh-CN": "zh-Hans",
    "zh-Hans": "zh-Hans",
    "zh-TW": "zh-Hant",
    "zh-Hant": "zh-Hant",
    "ko": "ko",
    "nl": "nl",
    "pt": "pt-PT",
    "pt-BR": "pt",
    "ru": "ru",
}
FLOAT_SAMPLE = 1234.5678
INTEGER_SAMPLE_TYPES = set("bcdoxXn")
FLOAT_SAMPLE_TYPES = set("eEfFgG%")


class CatalogError(Exception):
    pass


class BorealisField:
    def __init__(self, index, spec):
        self.index = index
        self.spec = spec

    def sample(self):
        if self.spec and self.spec[-1] in FLOAT_SAMPLE_TYPES:
            return FLOAT_SAMPLE
        if self.spec and self.spec[-1] in INTEGER_SAMPLE_TYPES:
            return 1234
        return f"⟨{self.index}⟩"

    def shown(self):
        return format(self.sample(), self.spec)


class BorealisText:
    def __init__(self, key, text):
        self.key = key
        self.text = text
        self.fields = self.parse_fields()

    def parse_fields(self):
        if "{{" in self.text or "}}" in self.text:
            raise CatalogError(f"{self.key}: escaped braces have no spelling in an arisa catalog")
        fields = []
        automatic = None
        try:
            parsed = list(string.Formatter().parse(self.text))
        except ValueError as failure:
            raise CatalogError(f"{self.key}: {failure}") from failure
        for _, name, spec, conversion in parsed:
            if name is None:
                continue
            if conversion:
                raise CatalogError(f"{self.key}: conversion !{conversion} is not supported")
            if name == "":
                if automatic is False:
                    raise CatalogError(f"{self.key}: mixes automatic and numbered fields")
                automatic = True
                index = len(fields)
            elif name.isdigit():
                if automatic is True:
                    raise CatalogError(f"{self.key}: mixes automatic and numbered fields")
                automatic = False
                index = int(name)
            else:
                raise CatalogError(f"{self.key}: named field {{{name}}} is not supported")
            fields.append(BorealisField(index, spec or ""))
        return fields

    def argument_count(self):
        return max((field.index for field in self.fields), default=-1) + 1

    def arisa_pattern(self):
        pattern = []
        fields = iter(self.fields)
        for literal, name, _, _ in string.Formatter().parse(self.text):
            pattern.append(literal)
            if name is not None:
                pattern.append("{" + str(next(fields).index) + "}")
        return "".join(pattern)

    def dropped_specs(self):
        return [field for field in self.fields if field.spec]

    def rendered(self):
        samples = {}
        for field in self.fields:
            samples[field.index] = field.sample()
        arguments = [samples.get(index, "") for index in range(self.argument_count())]
        return self.text.format(*arguments)

    def arisa_arguments(self):
        shown = {}
        for field in self.fields:
            shown[field.index] = field.shown()
        return [shown.get(index, "") for index in range(self.argument_count())]


class BorealisCatalog:
    def __init__(self, locale):
        self.locale = locale
        self.entries = {}
        for name in AKIRA_CATALOG_FILES:
            path = BOREALIS_CATALOGS / locale / name
            if path.exists():
                self.flatten(Path(name).stem, json.loads(path.read_text(encoding="utf-8")), path)

    def flatten(self, prefix, node, path):
        if isinstance(node, dict):
            for name, child in node.items():
                if "/" in name or "~" in name:
                    raise CatalogError(f"{path}: key {name!r} needs JSON pointer escaping")
                self.flatten(f"{prefix}/{name}", child, path)
            return
        if not isinstance(node, str):
            raise CatalogError(f"{path}: {prefix} is not a string")
        self.entries[prefix] = BorealisText(prefix, node)

    @staticmethod
    def locales():
        found = []
        for directory in sorted(BOREALIS_CATALOGS.iterdir()):
            if any((directory / name).exists() for name in AKIRA_CATALOG_FILES):
                if directory.name not in ARISA_LOCALE_IDENTIFIERS:
                    raise CatalogError(f"{directory.name} has no arisa locale identifier")
                found.append(directory.name)
        return found

    def shown_with(self, default):
        shown = dict(default.entries)
        shown.update(self.entries)
        return shown


class PluralKey:
    @staticmethod
    def split(key):
        for form in PLURAL_FORMS:
            suffix = "_" + form
            if key.endswith(suffix):
                return key[: -len(suffix)], form
        return key, None

    @staticmethod
    def form_of(key, shown):
        base, form = PluralKey.split(key)
        if form is None or base not in shown:
            return None
        return base, form


class ArisaCatalogWriter:
    def __init__(self, shown):
        self.shown = shown

    def entries(self):
        written = {}
        for key in sorted(self.shown):
            if PluralKey.form_of(key, self.shown) is not None:
                continue
            forms = {form: self.shown[key + "_" + form].arisa_pattern()
                     for form in PLURAL_FORMS if key + "_" + form in self.shown}
            if forms:
                forms["other"] = self.shown[key].arisa_pattern()
                written[key] = forms
            else:
                written[key] = self.shown[key].arisa_pattern()
        return written

    @staticmethod
    def quoted(text):
        encoded = json.dumps(text, ensure_ascii=False)
        return encoded.replace("\x7f", "\\u007f")

    def source(self):
        lines = []
        for key, value in self.entries().items():
            if isinstance(value, dict):
                forms = ", ".join(f"{form} = {self.quoted(text)}" for form, text in value.items())
                lines.append(f"{self.quoted(key)} = {{ {forms} }}")
            else:
                lines.append(f"{self.quoted(key)} = {self.quoted(value)}")
        return "\n".join(lines) + "\n"


class ArisaInterpolation:
    def __init__(self, pattern):
        self.pattern = pattern

    def segments(self, count):
        found = []
        at = 0
        literal = 0
        following = 0
        while at < len(self.pattern):
            if self.pattern[at] == "{":
                close = self.pattern.find("}", at)
                if close != -1:
                    inside = self.pattern[at + 1 : close]
                    numbered = inside != "" and all("0" <= digit <= "9" for digit in inside)
                    if inside == "" or numbered:
                        index = int(inside) if numbered else following
                        if index < count:
                            if at > literal:
                                found.append(("literal", self.pattern[literal:at]))
                            found.append(("argument", index))
                            following = index + 1
                            at = close + 1
                            literal = at
                            continue
            at += 1
        if literal < len(self.pattern):
            found.append(("literal", self.pattern[literal:]))
        return found

    def rendered(self, arguments):
        pieces = []
        for kind, value in self.segments(len(arguments)):
            pieces.append(arguments[value] if kind == "argument" else value)
        return "".join(pieces)


class ArisaCatalogReader:
    def __init__(self, path):
        self.path = path
        self.entries = tomllib.loads(path.read_text(encoding="utf-8"))

    def pattern(self, key):
        if key in self.entries:
            entry = self.entries[key]
            if isinstance(entry, dict):
                return entry["other"]
            return entry
        base, form = PluralKey.split(key)
        if form is not None:
            entry = self.entries.get(base)
            if isinstance(entry, dict):
                return entry.get(form, entry["other"])
        return None


class StringCatalogConverter:
    def __init__(self):
        self.default = BorealisCatalog(DEFAULT_LOCALE)
        self.catalogs = [BorealisCatalog(locale) for locale in BorealisCatalog.locales()]

    @staticmethod
    def target(locale):
        return ARISA_CATALOGS / f"{ARISA_LOCALE_IDENTIFIERS[locale]}.toml"

    def sources(self):
        return {self.target(catalog.locale): ArisaCatalogWriter(catalog.shown_with(self.default)).source()
                for catalog in self.catalogs}

    def report_notes(self):
        for catalog in self.catalogs:
            missing = len(self.default.entries.keys() - catalog.entries.keys())
            if missing:
                print(f"note: {catalog.locale} takes {missing} keys from {DEFAULT_LOCALE}, as borealis falls back")
            for text in catalog.entries.values():
                for field in text.dropped_specs():
                    print(f"note: {catalog.locale} {text.key} argument {field.index} drops spec "
                          f":{field.spec}; pass it already formatted")

    def write(self):
        ARISA_CATALOGS.mkdir(parents=True, exist_ok=True)
        for path, source in self.sources().items():
            path.write_text(source, encoding="utf-8")
            print(f"wrote {path.relative_to(REPOSITORY)}")
        self.report_notes()

    def check(self):
        failures = []
        for path, source in self.sources().items():
            if not path.exists() or path.read_text(encoding="utf-8") != source:
                failures.append(f"{path.relative_to(REPOSITORY)} is stale: run scripts/convert-i18n.py")
        compared = 0
        for catalog in self.catalogs:
            reader = ArisaCatalogReader(self.target(catalog.locale))
            for key, text in sorted(catalog.shown_with(self.default).items()):
                pattern = reader.pattern(key)
                if pattern is None:
                    failures.append(f"{catalog.locale} {key}: missing from {reader.path.name}")
                    continue
                expected = text.rendered()
                actual = ArisaInterpolation(pattern).rendered(text.arisa_arguments())
                if expected != actual:
                    failures.append(f"{catalog.locale} {key}: borealis {expected!r} arisa {actual!r}")
                compared += 1
        for failure in failures:
            print(f"error: {failure}", file=sys.stderr)
        if failures:
            return 1
        print(f"string catalogs match: {compared} keys across {len(self.catalogs)} locales")
        return 0


def main():
    parser = argparse.ArgumentParser(description="Convert akira's borealis i18n json to arisa TOML string catalogs")
    parser.add_argument("--check", action="store_true",
                        help="verify the arisa catalogs are current and render every borealis key identically")
    options = parser.parse_args()
    try:
        converter = StringCatalogConverter()
        if options.check:
            return converter.check()
        converter.write()
        return 0
    except CatalogError as failure:
        print(f"error: {failure}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
