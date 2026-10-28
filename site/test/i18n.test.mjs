// Checks that every translatable string on the website exists in every language.
// Run: node --test site/test/
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

import {translations, SUPPORTED} from '../i18n.js';

const html = readFileSync(new URL('../index.html', import.meta.url), 'utf8');

// Keys set from JavaScript rather than markup.
const SCRIPT_KEYS = ['meta.description', 'js.odo'];

const english = new Map();
for (const m of html.matchAll(/<(\w+)\b[^>]*\bdata-i18n="([^"]+)"[^>]*>([\s\S]*?)<\/\1>/g)) {
    if (!english.has(m[2])) english.set(m[2], m[3]);
}
for (const m of html.matchAll(/data-i18n-attr="[\w-]+:([^"]+)"/g)) english.set(m[1], '');
for (const key of SCRIPT_KEYS) english.set(key, '');

const tagCounts = (s) => {
    const counts = {};
    for (const m of s.matchAll(/<(strong|em|code|span|b|i|br)\b/g)) counts[m[1]] = (counts[m[1]] || 0) + 1;
    return counts;
};

test('languages offered match the selector buttons', () => {
    const buttons = [...html.matchAll(/data-lang="(\w+)"/g)].map((m) => m[1]);
    assert.deepEqual(buttons, SUPPORTED);
    assert.deepEqual(Object.keys(translations).sort(), SUPPORTED.filter((l) => l !== 'en').sort());
});

test('the page has translatable keys', () => {
    assert.ok(english.size > 100, `only ${english.size} keys found`);
});

for (const lang of Object.keys(translations)) {
    const dict = translations[lang];

    test(`${lang}: every key on the page is translated`, () => {
        const missing = [...english.keys()].filter((k) => typeof dict[k] !== 'string' || dict[k].trim() === '');
        assert.deepEqual(missing, []);
    });

    test(`${lang}: no stale keys`, () => {
        const stale = Object.keys(dict).filter((k) => !english.has(k) && !k.startsWith('anchor.'));
        assert.deepEqual(stale, []);
    });

    test(`${lang}: inline markup matches English`, () => {
        const mismatched = [];
        for (const [key, en] of english) {
            if (!en || typeof dict[key] !== 'string') continue;
            if (JSON.stringify(tagCounts(en)) !== JSON.stringify(tagCounts(dict[key]))) mismatched.push(key);
        }
        assert.deepEqual(mismatched, []);
    });
}

for (const lang of Object.keys(translations)) {
    test(`${lang}: docs anchor for the pulse signal exists`, () => {
        const doc = readFileSync(new URL(`../../docs/${lang}/01-hardware-and-wiring.md`, import.meta.url), 'utf8');
        const slugs = [...doc.matchAll(/^#{1,6} (.+)$/gm)].map((m) =>
            m[1].trim().toLowerCase().replace(/[^\p{L}\p{N}\s-]/gu, '').replace(/\s/g, '-'));
        assert.ok(slugs.includes(translations[lang]['anchor.signal']), `${translations[lang]['anchor.signal']} not in ${slugs}`);
    });
}
