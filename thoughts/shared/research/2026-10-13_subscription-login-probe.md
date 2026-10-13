---
date: 2026-10-13
topic: amiga-pi Phase O item O1 - feasibility of subscription logins (browser-free login and refresh on the Amiga)
tags: [amiga-pi, oauth, device-code, chatgpt, codex, github-copilot, kimi, xai, meta, terms, phase-o]
status: final
---

# Subscription login feasibility probe (O1)

Host-only study. Method: read of pi 1.1.0 pinned source (`/Users/spot/Code/amiga-pi/vendor/pi-src`, commit `abe508e`), provider public documentation fetched 2026-10-13, and unauthenticated TLS handshakes (`openssl s_client`, no request sent, no credentials, no login attempt). No code written, nothing authenticated.

Paths below are relative to `vendor/pi-src/packages/`. `oauth/` = `ai/src/auth/oauth/`.

Owner decision applied: no Anthropic provider (`ai/src/auth/oauth/anthropic.ts` and `ai/src/providers/anthropic.ts:83` exist in pi, deliberately not studied).

## Providers pi 1.1.0 can `/login` to (minus Anthropic)

Counted from the provider definitions that carry an `oauth:` entry (`ai/src/providers/*.ts`) and the loader table (`oauth/load.ts:12-22`, nine loaders), not by grepping one spelling:

| pi provider id | Display name | Flow module |
|---|---|---|
| `openai` | OpenAI (ChatGPT subscription), "Sign in with ChatGPT" (`ai/src/providers/openai.ts:12-22`) | `oauth/openai-chatgpt.ts` |
| `openai-codex` | OpenAI Codex (legacy), "OpenAI (ChatGPT Plus/Pro)" (`ai/src/providers/openai-codex.ts:9-18`) | `oauth/openai-codex.ts` |
| `github-copilot` | GitHub Copilot (`ai/src/providers/github-copilot.ts:16`) | `oauth/github-copilot.ts` |
| `kimi-coding` | Kimi Code (subscription) (`ai/src/providers/kimi-coding.ts`) | `oauth/kimi-coding.ts` |
| `xai` | xAI (Grok/X subscription) (`ai/src/providers/xai.ts:12-18`) | `oauth/xai.ts` |
| `meta` | Meta (Muse subscription) (`ai/src/providers/meta.ts:16`) | `oauth/meta.ts` |
| `openrouter` | not a subscription: yields a long-lived API key (`oauth/openrouter.ts:5, 19-20, 109`) | out of scope for the subscription gate; listed for completeness |
| radius gateway | not a subscription: self-hosted gateway login with client id `pi-gateway` (`oauth/radius.ts:25`) | out of scope |

`isSubscription: true` is set on openai, openai-codex, github-copilot, kimi-coding, xai, meta (and anthropic).

## Shared facts

**Storage (all providers).** `auth.json` is a JSON object keyed by pi provider id; each value is a type-tagged `Credential` (`ai/src/auth/types.ts:17-37`): `{"type":"oauth","access":<string>,"refresh":<string>,"expires":<epoch ms>, ...extra}` (extra fields allowed, `[key: string]: unknown` at `types.ts:24-29`) or `{"type":"api_key","key":...}`. File is written mode 0600 on creation (`coding-agent/src/core/auth-storage.ts:25`), guarded by `proper-lockfile` (`auth-storage.ts:9, 128`), default path `<agentDir>/auth.json` (`auth-storage.ts:347`). Extra fields per provider are listed in each section; amiga-pi must round-trip unknown fields.

**Device-code polling** (`oauth/device-code.ts`): RFC 8628; default interval 5 s when the server omits it, `slow_down` adds 5 s (comments at lines 6-8).

**Refresh contract.** `OAuthAuth.refresh(credential, signal)` runs under the store lock (`types.ts:233-237`). pi refreshes on expiry; the stored `expires` already has a safety margin subtracted by some providers (noted per provider).

**Transport (probed 2026-10-13 from this Mac, OpenSSL 3.6.4, handshake only).** Every endpoint host used below (auth.openai.com, chatgpt.com, api.openai.com, github.com, api.github.com, api.individual.githubcopilot.com, auth.x.ai, api.x.ai, auth.kimi.com, api.kimi.com, auth.meta.com, api.meta.ai) refused TLS 1.0 and 1.1 and accepted TLS 1.2 and TLS 1.3. Negotiated TLS 1.2 suites when the client offered the OpenSSL default list: `ECDHE-ECDSA-AES128-GCM-SHA256` (openai, github, x.ai), `ECDHE-RSA-AES128-GCM-SHA256` (githubcopilot.com, meta), `ECDHE-ECDSA-CHACHA20-POLY1305` (kimi). So AmiSSL needs: TLS 1.2 minimum, ECDHE key exchange, AES-GCM and ChaCha20-Poly1305, ECDSA (P-256 at least) and RSA server certificates, SNI. Not probed: which weaker suites a server would also accept, certificate chain roots (the Amiga needs a current CA bundle; not checked), HTTP/2 requirements (pi uses `fetch`; nothing in the source demands HTTP/2). Unconfirmed: that AmiSSL's OpenSSL build offers these suites; that is an O2 build-time check.

**Crypto needs across providers.** PKCE S256 (32 random bytes, SHA-256, base64url) for the authorization-code flows (`oauth/pkce.ts:17-33`). JWT payload decode (no signature check) for Codex account id (`oauth/openai-codex.ts:99-109`; note pi uses plain `atob` on a base64url segment, an Amiga port should use a real base64url decode). Everything else is JSON or form-encoded POSTs. No client secret anywhere; all public clients.

---

## 1. ChatGPT, provider `openai` ("Sign in with ChatGPT")

**Flow.** Authorization code + PKCE with dynamic client registration; redirect to `http://127.0.0.1:1455/auth/callback`. pi opens a local callback server (`oauth/openai-chatgpt.ts:81-132`) and in parallel prompts the user to paste the final redirect URL (`:271-279`). There is no device-code flow for this provider. Browser needed on the machine running the client: no. The user opens the printed URL on another device, signs in, the browser's redirect to 127.0.0.1:1455 fails to load there, and the user copies the full address bar URL back (`authorizationResultFromManualInput`, `:60-76`, requires origin and path to match the redirect URI). So login is browser-free on the Amiga side, but is "paste a URL", not "type a short code". The pasted URL must carry `code`, `state` and an issued `client_id` (`:52-62`).

**Endpoints.** Authorize `https://auth.openai.com/api/accounts/authorize` (`:19`), token `https://auth.openai.com/api/accounts/oauth/token` (`:20`), resource `https://api.openai.com/v1` (`:21`).

**Client id.** Dynamic: the authorize request sends placeholder `client_id=dynamic_agent_client` (`:16, 252`) plus `agent_name_hint` (`:253`) and `ext_agent_host_id=urn:uuid:<device uuid>` (`:226-231, 254`); OpenAI returns the issued client id in the callback (`:15, 59`). It is stored per credential as `clientId` (`:178`) and is required for refresh (`:208-212`). pi requires a device-id UUID from the host application (`:228`), so amiga-pi must generate and persist a UUID.

**Scopes.** `openid profile email offline_access resource.invoke chatgpt.tokens.use.direct` (`:26-27`); login fails if the grant lacks `chatgpt.tokens.use.direct` (`:170-172`). Also sends `nonce` and `resource` (`:257, 262`).

**Token lifetime and refresh.** Lifetime is whatever `expires_in` the server returns (`:166-167`); pi stores `Date.now() + expires_in*1000 - 180000` (3 min margin, `:29, 177`). Actual `expires_in` value: not observed (would need a real login); not stated in the docs read. Refresh: `POST` form to the token URL with `grant_type=refresh_token`, `client_id`, `refresh_token`, `resource` (`:213-219`); no browser. Response must include a new `refresh_token` and `scope` (`:155-165`), so refresh tokens rotate and the new one must be persisted every time.

**auth.json shape** (key `openai`): `{type:"oauth", access, refresh, expires, clientId, scopes:[...]}` (`:173-180`). The access token is used as the API key (`:306-308`); per `coding-agent/docs/models.md:144` these tokens only reach the Responses API.

**Transport.** TLS 1.2+ as above; PKCE S256; ID token must be present in the exchange response (`:202-204`) but is not parsed.

**Terms (facts).**
- OpenAI Help Center article "Using your ChatGPT plan in other apps and sites", https://help.openai.com/en/articles/20001542-using-your-chatgpt-plan-in-other-apps-and-sites . Direct fetch returned HTTP 403 (2026-10-13); content read through a search-result summary of that page, not verbatim, so these are paraphrases: select "Continue with ChatGPT / Sign in with ChatGPT" in the participating app; "All users can connect their account with supported open source tools"; Plus and Pro users can also connect eligible commercial tools; shared requests count toward the ChatGPT Work and Codex usage in the plan, with per-app caps in Settings > Usage; signing in does not give the app access to conversations or memories nor share an API key.
- Press report dated 2026-09-29 (RuntimeWire, https://runtimewire.com/article/openai-launches-login-with-chatgpt-and-routes-plan-usage-into-third-party-ai-app , read 2026-10-13) quotes: "Plus and Pro users can authorize eligible requests in tools including Notion, Vercel and Devin." It links the help article above as its only OpenAI primary source. The page does not define "eligible".
- Not confirmed: OpenAI's own announcement or developer-docs page for the open-source path; whether pi is a "supported open source tool" (no list found; searched the help article summary, learn.chatgpt.com/docs/auth, and web search; developers.openai.com did not appear in search results); the OpenAI Terms of Use (https://openai.com/policies/row-terms-of-use/ returned HTTP 403, not read).
- pi's own docs (`coding-agent/docs/providers.md`) make no terms claim about this provider.

**Open questions.** Is pi covered by "supported open source tools"? Does a program that registers dynamically from an Amiga count as a separate "agent host" per device UUID (rate or count limits unknown)? Real `expires_in` and refresh-token lifetime (needs one real login, owner step O6).

---

## 2. ChatGPT / Codex, provider `openai-codex` ("OpenAI (ChatGPT Plus/Pro)", marked legacy)

**Flow.** pi offers two methods in a select prompt (`oauth/openai-codex.ts:413-421`):
1. Device code ("headless"): `POST https://auth.openai.com/api/accounts/deviceauth/usercode` with JSON `{client_id}` (`:27, 187-193`) returns `device_auth_id`, `user_code`, `interval` (`:207-228`). pi shows the user code and URI `https://auth.openai.com/codex/device` (`:29, 343-349`), then polls `POST .../deviceauth/token` with `{device_auth_id,user_code}` (`:28, 231-245`) until it returns `authorization_code` + `code_verifier` (`:247-258`); timeout 15 min (`:31`). pi then exchanges at `/oauth/token` with `redirect_uri=https://auth.openai.com/deviceauth/callback` (`:30, 351-356`). Browser needed on the Amiga: no. The user types the short code on another device.
2. Browser: authorization code + PKCE, `redirect_uri=http://localhost:1455/auth/callback`, local callback server with fallback to pasted redirect URL/code (`:359-400`). Not needed if device code works.

**Endpoints.** `https://auth.openai.com` + `/oauth/authorize`, `/oauth/token` (`:23-25`), device paths above.

**Client id.** Fixed: `app_EMoamEEZ73f0CkXaXp7hrann` (`:22`). The authorize URL also sends `codex_cli_simplified_flow=true` and `originator=<agent name, default "pi">` (`:303-305`). The comment at `:364` says port 1455 is shared with the Codex CLI, i.e. this is the Codex CLI's public client id used by pi. Whether OpenAI regards this as permitted for third parties: not stated anywhere I could read (see Terms).

**Scopes.** `openid profile email offline_access` (`:34`).

**Token lifetime and refresh.** `expires = now + expires_in*1000` (`:138-142`; no margin here), `expires_in` server-supplied, value unobserved. Refresh: `POST` form `grant_type=refresh_token`, `refresh_token`, `client_id` to `/oauth/token` (`:167-185`); no browser. New refresh token expected in every response (`:134`), so persist each time. OpenAI's Codex docs say only "Codex refreshes tokens automatically during use before they expire" and give no lifetime (https://learn.chatgpt.com/docs/auth , read 2026-10-13; developers.openai.com/codex/auth redirects there with HTTP 308).

**auth.json shape** (key `openai-codex`): `{type:"oauth", access, refresh, expires, accountId}` (`:323-329`). `accountId` is read from the JWT claim `https://api.openai.com/auth`.`chatgpt_account_id` in the access token (`:35, 310-315`); login fails if absent (`:319-321`). Access token is the bearer for `https://chatgpt.com/backend-api` (`ai/src/providers/openai-codex.ts:12`).

**Transport.** TLS 1.2+; JWT payload decode (base64url + JSON); PKCE only for the browser method (device flow gets `code_verifier` from the server).

**Terms (facts).**
- Codex auth doc, https://learn.chatgpt.com/docs/auth (read 2026-10-13): "In these situations, prefer device code authentication (beta)." and "Enable device code login in your ChatGPT security settings (personal account)." The device-code method is therefore a beta that a personal account must turn on, and workspace admins control it separately. Also: "Treat `~/.codex/auth.json` like a password: it contains access tokens." The page has no statement about third-party tools using these tokens or this client id.
- Web summaries of community plugins (opencode-openai-codex-auth and others) say OpenAI "sanctions this auth flow for Codex tooling" and that use is personal-only; these are secondary, unofficial claims (https://www.assistant-ui.com/docs/guides/chatgpt-subscription , https://github.com/jxstanford/opencode-openai-codex-auth , found by search 2026-10-13, not fetched). Not an OpenAI statement.
- Codex CLI repo is Apache-2.0 (https://github.com/openai/codex/blob/main/LICENSE), which licenses the code, not the use of the service or the client id.
- Not confirmed: any OpenAI term that permits or forbids a non-Codex program to present Codex's client id. Looked in: learn.chatgpt.com/docs/auth, github.com/openai/codex docs, web search. OpenAI Terms of Use returned 403.

**Open questions.** Does the device-code toggle need to be enabled per account (yes per doc; the user must do it before first login)? Client-id use by a non-Codex client is unaddressed; section 1 (`openai`) is the route aligned with OpenAI's stated third-party program. Real token lifetimes.

---

## 3. GitHub Copilot, provider `github-copilot`

**Flow.** GitHub OAuth device flow (RFC 8628) (`oauth/github-copilot.ts:206-311, 434-485`). pi first asks for an optional GitHub Enterprise domain (`:435-445`). Browser on Amiga: no; the user enters the code at the returned `verification_uri` (github.com/login/device) on another device.

**Endpoints** (`:52-60`): `https://{domain}/login/device/code`, `https://{domain}/login/oauth/access_token`, and the Copilot token exchange `https://api.{domain}/copilot_internal/v2/token`. Inference base URL is derived from the Copilot token, default `https://api.individual.githubcopilot.com` (`getBaseUrlFromToken` `:69-76`, `getGitHubCopilotBaseUrl` `:78-87`; `toAuth` at `:501-506`).

**Client id.** Fixed, stored base64-obfuscated: decodes to `Iv1.b507a08c87ecfe98` (`:11`; decoded by me). It is the GitHub Copilot Chat (VS Code) OAuth app id; pi sends headers imitating VS Code Copilot Chat: `User-Agent: GitHubCopilotChat/0.35.0`, `Editor-Version: vscode/1.107.0`, `Editor-Plugin-Version: copilot-chat/0.35.0`, `Copilot-Integration-Id: vscode-chat` (`:13-18`), device flow also sends the User-Agent (`:213, 280`).

**Scope.** `read:user` (`:217`).

**Token lifetime and refresh.** The long-lived credential is the GitHub OAuth access token, stored as `refresh` (`:343`) (the device flow returns no `expires_in` for it that pi uses). Short-lived "access" is the Copilot token from `GET copilot_internal/v2/token` with `Authorization: Bearer <github token>` (`:321-328`); its `expires_at` (epoch seconds) is stored as `expires = expires_at*1000 - 5 min` (`:345`). Actual Copilot token lifetime: not observed (commonly reported as about 30 minutes, unconfirmed). Refresh = repeat that GET with the stored GitHub token; no browser; the GitHub token itself is not rotated. If the GitHub token is revoked, only a new device login helps. pi's refresh also calls the models endpoint (`:359-362`, best effort for `availableModelIds`) and at login enables model policies by `POST {base}/models/{id}/policy` (`:373-408, 472-480`).

**auth.json shape** (key `github-copilot`): `{type:"oauth", refresh:<github token>, access:<copilot token>, expires, enterpriseUrl?, availableModelIds:[...]}` (`:341-347, 363-366, 481-484`).

**Transport.** TLS 1.2+ (probed); plain form/JSON; the Copilot token is a `;`-delimited string whose `proxy-ep=` field is parsed for the API host (`:65-76`). No JWT or PKCE.

**Terms (facts).**
- GitHub Terms of Service, effective April 27, 2026, https://docs.github.com/en/site-policy/github-terms/github-terms-of-service (read 2026-10-13): Section H applies to "use of the API through a third party product that accesses GitHub"; "Abuse or excessively frequent requests to GitHub via the API may result in the temporary or permanent suspension"; "You may not share API tokens to exceed GitHub's rate limitations." Section J covers AI features and does not address client identity.
- GitHub Additional Products and Features terms, version effective August 27, 2026, https://docs.github.com/en/site-policy/github-terms/github-terms-for-additional-products-and-features : the Copilot section only says which terms govern ("Section J (AI Features) of the GitHub Terms of Service" for individual plans; Generative AI Services Terms for Business/Enterprise). No clause about unsupported clients.
- Acceptable Use Policies, https://docs.github.com/en/site-policy/acceptable-use-policies/github-acceptable-use-policies : prohibits "using our servers for any form of excessive automated bulk activity" and attempts "to gain unauthorized access to, any service, device, data, account or network"; no clause on third-party clients.
- Not confirmed: whether `copilot_internal/v2/token` and the imitated VS Code headers are permitted for non-GitHub clients. The endpoint is undocumented (name contains `internal`); search found only third-party notes that it is undocumented and "may violate GitHub's terms of service depending on your use case" (hexdocs/req-llm, litellm, fast.io pages, not authoritative). Not read: GitHub Generative AI Services Terms; GitHub Copilot product-specific terms beyond the page above. Official client-impersonation (User-Agent, Editor-Version, client id of VS Code's app) is a fact in pi's code and a question for the owner.

**Open questions.** Copilot token real lifetime; terms for non-GitHub clients using the VS Code app id and internal token endpoint; Copilot Business/Enterprise policy differences.

---

## 4. Kimi Code, provider `kimi-coding`

**Flow.** RFC 8628 device flow against `https://auth.kimi.com` (`oauth/kimi-coding.ts:4, 15, 266-278`). pi shows `verification_uri_complete` (the URL with the code embedded) (`:273`). Browser on Amiga: no. Host overridable by an env helper (`getOauthHost()`, `:36`), default `DEFAULT_OAUTH_HOST` (`:15`).

**Endpoints.** `POST {host}/api/oauth/device_authorization` (`:71`), token poll and refresh at `{host}/api/oauth/token` (`:228`, poll at `:155-165`). API: `https://api.kimi.com/coding` (`ai/src/providers/kimi-coding.ts:11`, Anthropic-messages protocol).

**Client id.** `17e5f671-d194-4dfb-9706-5516cb48c098` (`:14`). Scopes: none sent.

**Token lifetime and refresh.** `expires = now + expires_in*1000` (server `expires_in`, `:123-138`, value unobserved; no margin). Refresh: `POST` form `client_id`, `grant_type=refresh_token`, `refresh_token` (`:233-236`), up to 3 retries on network and 5xx (`:19, 241-262`); 401/403/`invalid_grant` means dead credential, re-login (`:250-256`). No browser. A new refresh token is returned and stored (`:78`).

**auth.json shape** (key `kimi-coding`): `{type:"oauth", access, refresh, expires}` (`:288-289`). Access is sent as `Authorization: Bearer` (`:292-294`).

**Transport.** TLS 1.2+ (kimi hosts negotiated ChaCha20-Poly1305 ECDSA on 1.2; AmiSSL must offer ChaCha20 or AES-GCM ECDSA). No PKCE, no JWT.

**Terms (facts).**
- Kimi help, "Using with third-party coding agents" (https://www.kimi.com/en/help/kimi-code/third-party-agents , read 2026-10-13): the "Third-party Tools" section only links guides for Claude Code, OpenCode, Codex and Hermes Agent; it has no rule text. A search summary of other Kimi pages (https://www.kimi.ai/help/kimi-code/third-party-agents , not fetched) says third-party use needs an API key created in the Kimi Console and that tampering with the client identifier (User-Agent) "will be treated as a violation and may result in suspension of your membership benefits"; second-hand, not verified verbatim.
- pi's flow logs in with device OAuth (no console API key) with a client id that belongs to Kimi's own CLI (assumed from the id and host; not confirmed). Whether membership OAuth tokens may be used by a non-Kimi client: not confirmed. Looked in: the two Kimi help pages and a web search; the Kimi/Moonshot terms of service not read.

**Open questions.** OAuth vs console-API-key route per Kimi's policy; token lifetimes.

---

## 5. xAI Grok (SuperGrok / X Premium), provider `xai`

**Flow.** RFC 8628 device flow (`oauth/xai.ts:1-3`). Uses `verification_uri_complete` when given (`:57` region of login). Browser on Amiga: no.

**Endpoints.** Device `https://auth.x.ai/oauth2/device/code` (`:10`), token `https://auth.x.ai/oauth2/token` (`:11`). API `https://api.x.ai/v1` (`ai/src/providers/xai.ts:12`; pi notes the subscription is routed through a "CLI chat proxy" per a third-party write-up, unverified; pi's provider uses the same `api.x.ai/v1` base in this file).

**Client id.** `b1a00492-073a-47ea-816f-4c329264a828` (`:8`); named scopes include `grok-cli:access` so this is the Grok CLI's client. Scopes: `openid profile email offline_access grok-cli:access api:access` (`:9`).

**Token lifetime and refresh.** `expires_in` from the server, default 3600 s if absent (`:13, 135-136`), minus 5 min skew (`:12, 141`). Refresh: `grant_type=refresh_token`, `client_id`, `refresh_token` (`:215-219`); xAI may omit a new `refresh_token` and pi then keeps the old one (`:131-134`). No browser.

**auth.json shape** (key `xai`): `{type:"oauth", access, refresh, expires}` (`:138-143`).

**Transport.** TLS 1.2+ (ECDSA AES-GCM); verification URI is forced to https (`:49-58`). No PKCE or JWT.

**Terms (facts).** No xAI primary source found. Looked in: web search (xAI terms, SuperGrok OAuth third-party). Only third-party tool docs (Hermes Agent, Kilo, OpenCode listings) say the flow works; Hermes' docs say xAI "enforces its own allowlist" on this OAuth surface and that some standard subscribers get HTTP 403 (secondary, https://hermes-agent.nousresearch.com/docs/ko/guides/xai-grok-oauth). xAI Terms of Service and Grok CLI docs not read.

**Open questions.** xAI terms for non-Grok-CLI clients; allowlist gating (403s).

---

## 6. Meta (Muse subscription), provider `meta`

**Flow.** RFC 8628 device flow against `https://auth.meta.com` (`oauth/meta.ts:3-4, 21-23`), then a key-mint call (`:24, 136-174`). Browser on Amiga: no.

**Endpoints.** `https://auth.meta.com/oidc/device/authorization/` and `/oidc/device/token/` (`:22-23`); mint `POST https://api.meta.ai/muse-code/key` with `Authorization: Bearer <identity token>` and `x-api-version: 1.0.0` (`:24, 138-147`).

**Client id.** `1031625952748946` ("Muse Code CLI client id", `:19-20`).

**Token lifetime and refresh.** The identity token is not renewable: the file header (`:9-12`) says `auth.meta.com` answers `grant_type=refresh_token` with 404 and issues no refresh token. pi stores the identity token as `refresh` and the minted API key as `access`; the key lives about a day (`API_KEY_LIFETIME_MS = 24h`, `:25`, set at `:173`). "Refresh" = call the mint endpoint again with the identity token (`:207-208`), which needs no browser while the identity token is valid. When mint returns 401/403 the session is dead and a new device login is required (`:157-164`). Identity token lifetime: unknown (not in source, no docs found); this determines how often the user must re-do the device login on the Amiga.

**auth.json shape** (key `meta`): `{type:"oauth", refresh:<identity token>, access:<minted api key>, expires}` (`:173`).

**Transport.** TLS 1.2+ (ECDHE-RSA AES-GCM, TLS 1.3 ChaCha20 offered). No PKCE or JWT.

**Terms (facts).** No Meta terms for this route read. Search found Meta's developer pages (https://developer.meta.com/ai/products/muse-code/ , https://developer.meta.com/ai/resources/blog/build-with-muse-spark/) and secondary claims: the Meta Model API is self-serve and OpenAI-SDK-compatible with its own API keys billed per token, and a discounted "Contributor" tier shares prompts with Meta for product improvement (secondary: https://wavect.io/blog/meta-muse-code-pricing-contributor-tier.md). Whether the minted key from a subscription may be used by a third-party client: not confirmed; those pages were not fetched.

**Open questions.** Identity token lifetime; whether the Contributor/data-sharing tier applies to keys minted this way; third-party-client terms; the pi file itself calls the minted key a "Model API" key, i.e. it may have a plain API-key alternative (`META_API_KEY`, `coding-agent/docs/providers.md:56`) with no login.

---

## Summary

"Browser-free on Amiga" = no browser and no localhost listener on the Amiga; the user completes sign-in on another device. "Terms finding" is what I could read; the owner decides.

| Provider (pi id) | Flow | Browser-free on Amiga (login) | Refresh browser-free | Terms finding | Open questions |
|---|---|---|---|---|---|
| ChatGPT `openai` | Auth code + PKCE, dynamic client registration, localhost redirect with paste-the-URL fallback; no device code | Yes, via paste of the full redirect URL (long URL, not a short code) | Yes (refresh token rotates every time) | OpenAI help article (paraphrased, 403 on direct fetch) says all users can connect "supported open source tools"; press 2026-09-29 reports rollout for third-party tools. Pi not shown on any list. Terms of Use unread (403) | Is pi "supported open source"? real `expires_in`; per-device UUID registration limits |
| ChatGPT/Codex `openai-codex` (legacy) | Device code (beta, must be enabled in account security settings) or localhost+paste; fixed Codex CLI client id | Yes (device code) | Yes (rotating refresh token) | OpenAI Codex auth doc: no statement on third-party clients; device code is "beta". Use of Codex's client id by pi: not addressed in any source read | Third-party use of Codex client id; lifetime unobserved |
| GitHub Copilot `github-copilot` | GitHub device flow, then `copilot_internal/v2/token` exchange | Yes | Yes (GitHub token long-lived; Copilot token re-fetched) | GitHub ToS (2026-04-27) has API third-party-product and abuse clauses; no clause on Copilot via non-GitHub clients; endpoint undocumented and client impersonates VS Code Copilot Chat | Does GitHub permit this client; Copilot token lifetime unobserved |
| Kimi Code `kimi-coding` | RFC 8628 device flow, fixed client id | Yes | Yes (rotating refresh token, 3 retries) | Kimi help page lists third-party guides without rule text; secondary report says third-party use needs a Console API key and client-identifier tampering risks suspension. Terms of service unread | OAuth vs Console API key route; lifetimes |
| xAI `xai` | RFC 8628 device flow, Grok CLI client id and scopes | Yes | Yes (refresh token may be reused) | No xAI primary source found; third-party docs report an xAI-side allowlist (403s) | xAI terms; allowlist |
| Meta `meta` | RFC 8628 device flow + key-mint call; no refresh grant | Yes | Partial: mint call is browser-free while the identity token is valid, but the identity token cannot be refreshed, so a device login recurs when it expires | No Meta terms read | Identity token lifetime (decides usability); terms; plain API key alternative exists |

Gate reading (login and refresh with no browser on the Amiga): technically met by all six, with Meta conditional on its identity-token lifetime and `openai` needing URL paste instead of a short code. The terms column is unconfirmed for every provider except the partial OpenAI statement; none of the six has a primary-source sentence that says "a third-party client may use this subscription via this OAuth client". Nothing here was verified by a real login (that is O6, owner).

## Not confirmed (and where I looked)

- Real token lifetimes (`expires_in`, refresh-token lifetime, Copilot token lifetime, Meta identity token lifetime): need a real login; not in source or the docs read.
- OpenAI Terms of Use and the help article (403 on direct fetch); OpenAI's own third-party announcement page (not found via search; only press coverage).
- GitHub Generative AI Services Terms and any Copilot third-party-client policy.
- Kimi/Moonshot ToS, xAI ToS and Grok CLI docs, Meta Muse/Model API terms.
- That AmiSSL's OpenSSL build and CA store cover the suites and roots above.

## Sources (read 2026-10-13)

- pi source: `vendor/pi-src` at abe508e (files cited inline).
- https://learn.chatgpt.com/docs/auth (Codex auth; redirect from https://developers.openai.com/codex/auth)
- https://help.openai.com/en/articles/20001542-using-your-chatgpt-plan-in-other-apps-and-sites (403 on fetch; search summary only)
- https://runtimewire.com/article/openai-launches-login-with-chatgpt-and-routes-plan-usage-into-third-party-ai-app (2026-09-29)
- https://docs.github.com/en/site-policy/github-terms/github-terms-of-service
- https://docs.github.com/en/site-policy/github-terms/github-terms-for-additional-products-and-features
- https://docs.github.com/en/site-policy/acceptable-use-policies/github-acceptable-use-policies
- https://www.kimi.com/en/help/kimi-code/third-party-agents
- https://hermes-agent.nousresearch.com/docs/ko/guides/xai-grok-oauth (secondary)
- https://developer.meta.com/ai/products/muse-code/ (surfaced by search, not fetched)

## Anthropic login, revisited (2026-10-13, after eriQue's reply)
eriQue: pi's Claude login bills "usage tokens" (usage credits), not the plan; it is not against the terms.
Official pages read via search 2026-10-13:
- support.claude.com 13189465 "Log in to your Claude account": paid subscribers with usage credits enabled may use certain third-party tools; that usage can be charged to credits instead of plan limits. Tools that misrepresent their identity to Anthropic's servers, or route third-party traffic against plan limits, are prohibited.
- support.claude.com 14246112 "Buy usage bundles": bundles apply to third-party products that use a Claude account.
- code.claude.com/docs/en/legal-and-compliance: developers building products use API keys (Console or a cloud provider); third-party developers may not offer Claude.ai login in their own apps or route requests through Free/Pro/Max credentials on users' behalf.
- support.claude.com 15036540 (Agent SDK, updated 2026-10-07): the June 2026 change was paused; claude -p, Agent SDK and third-party apps still draw from plan limits.
Verdict: eriQue is right that the billing goes to usage credits and that a user may sign in to a supported third-party tool. The bar on amiga-pi is the developer side: we would be offering Claude.ai login in our own app, and pi's flow presents Claude Code's client and the user:sessions:claude_code scope (identity). Decision unchanged: no Anthropic subscription login in amiga-pi; API key only. Not checked: whether Anthropic lists pi as a "certain third-party tool".
- Anthropic Support chat, 2026-10-13 (owner asked "is pi.dev a violation of terms?"): "Third-party developers must use API key authentication through the Claude Console or a supported cloud provider. They cannot offer Claude.ai login into their own applications or route requests through Free, Pro, or Max plan credentials." Also lists "routing requests through consumer subscriptions" under prohibited reselling/proxying. No verdict on pi itself without details. Confirms: no Claude login in amiga-pi.
