# MoJo Active

A personal-fork screen that renders Markdown fetched over Wi-Fi. The device is a
pure consumer: some other tool (a scheduled Claude Desktop task, a cron job, a
CI workflow) writes the pages and publishes them at URLs; the reader fetches
them and draws them.

Up to three documents, each its own tabbed section: **Priorities**, **Clients**
(one screen per client), and **News** (client-relevant news). Front Left/Right
switch sections, the side buttons page within one — the same tab pattern the
Settings screen uses. Only Priorities is required; a section with no URL has no
tab, and with a single section configured the tab bar is not drawn at all.

(The code is still named `dashboard` throughout — the store, the activity, the
cache file, the settings keys. Only the user-facing strings say MoJo Active.)

Not part of upstream CrossPoint — `SCOPE.md` rules out network-backed content
screens. It is kept to its own files so upstream merges stay clean:

| File | Role |
|---|---|
| `src/activities/dashboard/DashboardActivity.*` | The screen: fetch, cache, paginate, draw |
| `src/util/DashboardMarkdown.*` | Line → block classification (host-tested) |
| `src/DashboardStore.*` | The three configured URLs, persisted to `/.crosspoint/dashboard.json` |

Everything else is a few lines in the home menu, the settings list, and
`ActivityManager`.

## Setup

1. Publish one or two Markdown documents at **public HTTPS URLs**. No
   credentials are stored on the device, so a URL itself is the secret — use
   unguessable paths.
2. Set the URLs at `http://<device-ip>/settings`, under the *MoJo Active* card:
   - **Priorities URL** — required. Also settable on-device via
     **Settings → Priorities URL** if you would rather use the keyboard.
   - **Clients URL** — optional, web only. One `#` section per client.
   - **News URL** — optional, web only. Client-relevant news.
   - **Font Size** — Small / Medium / Large / Extra Large, web only. Medium is
     the default; Small is the original pairing. Larger sizes fit less on a
     screen, so a client section may spill onto a second screen.

   A URL without a scheme is stored as `https://`. Anything that is not an
   http(s) URL is rejected and the previous value is kept — note the web page
   reports "saved" either way, so reload it to confirm what actually stuck.
3. **Home → MoJo Active** (first item). It connects Wi-Fi if needed, fetches
   every configured section, and renders the first one.

Buttons: Back returns home, Confirm re-fetches every section, front Left/Right
change section, the side buttons page within a section. The corner shows `3/8`
when a section runs to more than one screen.

Sections are fetched and cached independently, so one failing does not affect
the others: a section that could not be refreshed shows its previous copy
marked `offline copy`, and one that has never been fetched says `section
unavailable` while the rest stay readable.

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

- **8 KB cap per section**, not shared: each document gets its own budget, and
  only the section being read is in memory. Roughly 130 lines. Beyond that the
  fetch stops and that section alone is marked `clipped`. With ~10 clients that
  is roughly 700 bytes each: a heading and a handful of bullets.
- **32 screens max per section**, so ~32 clients in the Clients section.
- **Long values wrap** under their label rather than being truncated, so keep
  stat values short — `12`, `3 open`, `2h 15m`.
- **Monochrome, ~800×480**, inset by the theme's content padding. No images, no
  colour, no layout control beyond the block types above.
- **Screen count moves with font size.** Raising the size re-paginates, so the
  `3/8` counter and where sections break both change.

## Offline behaviour

Opening the screen always reads the cached copy from the SD card first, then
tries to refresh it:

| On open | What happens |
|---|---|
| Already on Wi-Fi | Fetches both documents and replaces the cache |
| Not connected, a saved network in range | Joins it silently, then fetches |
| Not connected, no saved network reachable | Shows the cached sections marked `offline copy` — **no Wi-Fi picker** |
| Not connected and no section cached | Shows the Wi-Fi picker, since there is nothing else to display |

Pressing Confirm is an explicit "get me fresh data", so on that path the picker
*is* shown when the saved networks do not come up — that is the way to join a
new network from this screen.

## Behaviour when things go wrong

Each section is cached to its own file — `/.crosspoint/dashboard-priorities.md`,
`-clients.md`, `-news.md`. (The old combined `dashboard.md` is deleted on first
run.) If a fetch fails
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
> **News page.** Summarise news from the last business day that is relevant to
> my clients, under 3 KB. Start with `# News`, then one `##` heading per story
> with a bullet or two of why it matters. Same block types as above. Publish it
> to <news URL>.
>
> **Clients page.** For each client with activity in the last business day,
> write one section starting with `# <Client Name>` — this is what splits the
> screens, so exactly one `#` per client and never inside one. Under ~700 bytes
> per client: what shipped, what is blocked, what is next. Same block types as
> above. Order clients by how much I need to look at them. Publish the whole
> thing as one file to <clients URL>.
