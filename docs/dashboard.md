# MoJo Active

A personal-fork screen that renders Markdown fetched over Wi-Fi. The device is a
pure consumer: some other tool (a scheduled Claude Desktop task, a cron job, a
CI workflow) writes the pages and publishes them at URLs; the reader fetches
them and draws them.

Two documents make up one front-to-back sequence of screens: a **priorities**
page first, then an optional **clients** page holding one section per client.
Paging forward from the last priorities screen lands on the first client.

(The code is still named `dashboard` throughout — the store, the activity, the
cache file, the settings keys. Only the user-facing strings say MoJo Active.)

Not part of upstream CrossPoint — `SCOPE.md` rules out network-backed content
screens. It is kept to its own files so upstream merges stay clean:

| File | Role |
|---|---|
| `src/activities/dashboard/DashboardActivity.*` | The screen: fetch, cache, paginate, draw |
| `src/util/DashboardMarkdown.*` | Line → block classification (host-tested) |
| `src/DashboardStore.*` | The two configured URLs, persisted to `/.crosspoint/dashboard.json` |

Everything else is a few lines in the home menu, the settings list, and
`ActivityManager`.

## Setup

1. Publish one or two Markdown documents at **public HTTPS URLs**. No
   credentials are stored on the device, so a URL itself is the secret — use
   unguessable paths.
2. Set the URLs at `http://<device-ip>/settings`, under the *MoJo Active* card:
   - **Priorities URL** — required. Also settable on-device via
     **Settings → Priorities URL** if you would rather use the keyboard.
   - **Clients URL** — optional, web only. Leave empty to show priorities alone.

   A URL without a scheme is stored as `https://`. Anything that is not an
   http(s) URL is rejected and the previous value is kept — note the web page
   reports "saved" either way, so reload it to confirm what actually stuck.
3. **Home → MoJo Active** (first item). It connects Wi-Fi if needed, fetches
   both documents, and renders.

Buttons: Back returns home, Confirm re-fetches both documents, Up/Down move
between screens. The corner shows `3/8` when there is more than one screen.

The two documents are fetched independently. If the clients page fails but
priorities succeed, the priorities screens still render, marked `clients
unavailable` in the corner.

## Screens

**Every `#` heading starts a new screen.** That is the whole rule — no
page-break syntax, no per-client files:

```markdown
# Priorities          <- screen 1
- Ship the X4 build
- Review the Acme SOW

# Acme Corp           <- screen 2
## Yesterday
- Shipped checkout fix

# Globex              <- screen 3
...
```

A section longer than one display overflows onto further screens rather than
being clipped, so a wordy client costs two screens instead of losing content.
`##` headings do **not** break — use them freely to structure a client's screen.

## What a page may contain

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

- **8 KB cap across both documents combined.** Roughly 130 lines. Beyond that
  the fetch stops and the page is marked `clipped` in the corner — and since
  clients are fetched second, they are what gets lost. With ~10 clients that is
  roughly 700 bytes each: a heading and a handful of bullets. Publish a summary,
  not a data dump.
- **32 screens max**, so ~30 clients after the priorities screens.
- **Long values wrap** under their label rather than being truncated, so keep
  stat values short — `12`, `3 open`, `2h 15m`.
- **Monochrome, ~800×480.** No images, no colour, no layout control beyond the
  block types above.

## Behaviour when things go wrong

Both documents are cached, concatenated, to `/.crosspoint/dashboard.md`. If the
priorities fetch fails
— no Wi-Fi, server down, bad URL — the last good copy renders with `offline
copy` in the corner instead of an error screen. An error is only shown when
there is no cache to fall back on.

The cache is a plain text file: dropping one there by hand renders it on the
next open, which is the easy way to test layout without publishing anything.

## Suggested publisher prompt

For a scheduled Claude task, something like:

> **Priorities page.** Write my priority list as Markdown, under 3 KB, using
> only: `#`/`##` headings, `-` bullets, `Label: value` lines, and `---` rules.
> Start with `# Priorities`, then a line `Updated: <date time>`. Keep stat
> values under ~15 characters. No images, no nested lists, no code blocks.
> Publish it to <priorities URL>.
>
> **Clients page.** For each client with activity in the last business day,
> write one section starting with `# <Client Name>` — this is what splits the
> screens, so exactly one `#` per client and never inside one. Under ~700 bytes
> per client: what shipped, what is blocked, what is next. Same block types as
> above. Order clients by how much I need to look at them. Publish the whole
> thing as one file to <clients URL>.
