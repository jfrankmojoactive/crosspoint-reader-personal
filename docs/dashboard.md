# Dashboard

A personal-fork screen that renders a Markdown page fetched over Wi-Fi. The
device is a pure consumer: some other tool (a scheduled Claude Desktop task, a
cron job, a CI workflow) writes the page and publishes it at a URL; the reader
fetches that URL and draws it.

Not part of upstream CrossPoint — `SCOPE.md` rules out network-backed content
screens. It is kept to its own files so upstream merges stay clean:

| File | Role |
|---|---|
| `src/activities/dashboard/DashboardActivity.*` | The screen: fetch, cache, paginate, draw |
| `src/util/DashboardMarkdown.*` | Line → block classification (host-tested) |
| `src/DashboardStore.*` | The configured URL, persisted to `/.crosspoint/dashboard.json` |

Everything else is a few lines in the home menu, the settings list, and
`ActivityManager`.

## Setup

1. Publish a Markdown page at a **public HTTPS URL**. No credentials are stored
   on the device, so the URL itself is the secret — use an unguessable path.
2. On the device: **Settings → Dashboard URL**, type the URL, confirm.
3. **Home → Dashboard**. It connects Wi-Fi if needed, fetches, and renders.

Buttons: Back returns home, Confirm re-fetches, Up/Down page through a long
document.

## What the page may contain

The device does **not** implement Markdown. Each line maps to one block, so
anything unrecognised still renders as plain text rather than breaking. What is
recognised:

```markdown
# Wednesday                  <- large bold heading
## Top tasks                 <- bold heading

- Ship the dashboard build   <- bullet
1. Also a bullet             <- ordered markers render as bullets

Inbox: 12                    <- label left, value right-aligned
| Unread | 12 |              <- two-cell table row, same as above
| Open PRs | 3 |
|---|---|                    <- separator rows are dropped

---                          <- horizontal rule

Anything else is wrapped as a paragraph.
> Quotes render as plain text.
```

Inline `**bold**`, `__bold__`, `` `code` ``, and `[text](url)` markers are
stripped to their visible text. Single `*` and `_` are left alone so
`snake_case` and `2 * 3` survive intact.

**Put the generation time in the page itself** — e.g. `Updated: 2026-08-08
09:00`. The device has no reliable clock for this and deliberately does not
stamp one; a line the publisher writes is always accurate, and renders as a
key/value row.

## Limits worth designing around

- **8 KB cap.** Roughly 130 lines. Beyond that the fetch stops and the page is
  marked `clipped` in the corner. Publish a summary, not a data dump.
- **32 pages max** at ~20 lines a page.
- **Long values wrap** under their label rather than being truncated, so keep
  stat values short — `12`, `3 open`, `2h 15m`.
- **Monochrome, ~800×480.** No images, no colour, no layout control beyond the
  block types above.

## Behaviour when things go wrong

Every fetched page is cached to `/.crosspoint/dashboard.md`. If the fetch fails
— no Wi-Fi, server down, bad URL — the last good copy renders with `offline
copy` in the corner instead of an error screen. An error is only shown when
there is no cache to fall back on.

The cache is a plain text file: dropping one there by hand renders it on the
next open, which is the easy way to test layout without publishing anything.

## Suggested publisher prompt

For a scheduled Claude task, something like:

> Write my daily dashboard as Markdown, under 8 KB, using only: `#`/`##`
> headings, `-` bullets, `Label: value` lines, and `---` rules. Start with a
> line `Updated: <date time>`. Keep stat values under ~15 characters. No images,
> no nested lists, no code blocks. Publish it to <your URL>.
