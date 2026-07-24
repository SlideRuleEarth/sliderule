/**
 * SlideRule GitHub Device-Flow Authentication Plugin
 *
 * At opencode startup this plugin checks whether SLIDERULE_MCP_TOKEN is
 * already set (and not expired). If not, it runs the GitHub Device Flow
 * against the SlideRule authenticator at https://login.slideruleearth.io,
 * prints the user_code + verification URL to the console, polls until the
 * user authorises, and then injects the resulting JWT into every shell
 * environment via the shell.env hook so the MCP server config can reference
 * {env:SLIDERULE_MCP_TOKEN}.
 *
 * Endpoints used:
 *   POST https://login.slideruleearth.io/auth/github/device
 *   POST https://login.slideruleearth.io/auth/github/device/poll
 *
 * The poll response returns { status, token, metadata } on success.
 * The metadata.exp field is a Unix timestamp we use to detect stale tokens.
 */

const AUTH_BASE = "https://login.slideruleearth.io";
const DEVICE_URL = `${AUTH_BASE}/auth/github/device`;
const POLL_URL = `${AUTH_BASE}/auth/github/device/poll`;

/**
 * Decode the payload of a JWT without verifying the signature.
 * We only use this to read the exp claim locally.
 */
function decodeJwtPayload(token) {
  try {
    const parts = token.split(".");
    if (parts.length !== 3) return null;
    // base64url → base64 → JSON
    const padded = parts[1].replace(/-/g, "+").replace(/_/g, "/");
    const json = Buffer.from(padded, "base64").toString("utf8");
    return JSON.parse(json);
  } catch {
    return null;
  }
}

/**
 * Return true if the token exists and has more than 5 minutes of life left.
 */
function tokenIsValid(token) {
  if (!token) return false;
  const payload = decodeJwtPayload(token);
  if (!payload || !payload.exp) return false;
  const nowSec = Math.floor(Date.now() / 1000);
  return payload.exp - nowSec > 300; // 5-minute grace
}

/**
 * POST a JSON body and return the parsed response object.
 */
async function post(url, body) {
  const resp = await fetch(url, {
    method: "POST",
    headers: { "Content-Type": "application/json", Accept: "application/json" },
    body: JSON.stringify(body),
  });
  return resp.json();
}

/**
 * Run the full device flow, printing instructions and polling until done.
 * Returns the SlideRule JWT string on success, throws on failure.
 */
async function runDeviceFlow() {
  // Step 1 – request device code
  const init = await post(DEVICE_URL, {});
  if (init.error) {
    throw new Error(`Device code request failed: ${init.error_description ?? init.error}`);
  }

  const { device_code, user_code, verification_uri, verification_uri_complete, expires_in, interval } = init;
  const pollIntervalMs = (interval ?? 5) * 1000;
  const deadline = Date.now() + (expires_in ?? 900) * 1000;

  // Step 2 – show the user what to do
  console.log("\n============================================================");
  console.log("  SlideRule authentication required");
  console.log("============================================================");
  if (verification_uri_complete) {
    console.log(`  Open this URL in your browser:\n\n    ${verification_uri_complete}`);
  } else {
    console.log(`  1. Go to: ${verification_uri}`);
    console.log(`  2. Enter code: ${user_code}`);
  }
  console.log("\n  Waiting for you to authorise in the browser...");
  console.log("============================================================\n");

  // Step 3 – poll
  while (Date.now() < deadline) {
    await new Promise((r) => setTimeout(r, pollIntervalMs));

    const poll = await post(POLL_URL, { device_code });

    if (poll.status === "success" && poll.token) {
      console.log(`  Authenticated as ${poll.metadata?.sub ?? "unknown"} (role: ${(poll.metadata?.org_roles ?? []).join(", ") || "guest"})`);
      console.log("============================================================\n");
      return poll.token;
    }

    if (poll.error === "authorization_pending" || poll.error === "slow_down") {
      // still waiting — increase interval on slow_down
      if (poll.error === "slow_down" && poll.interval) {
        await new Promise((r) => setTimeout(r, (poll.interval - interval) * 1000));
      }
      continue;
    }

    // any other error is terminal
    throw new Error(`Authentication failed: ${poll.error_description ?? poll.error}`);
  }

  throw new Error("Device flow timed out — the code expired before you authorised.");
}

// ── Plugin export ─────────────────────────────────────────────────────────────

export const SlideRuleAuth = async ({ $ }) => {
  let token = process.env.SLIDERULE_MCP_TOKEN ?? "";

  if (!tokenIsValid(token)) {
    try {
      token = await runDeviceFlow();
      // Persist into the current process so subsequent tool calls also see it
      process.env.SLIDERULE_MCP_TOKEN = token;
    } catch (err) {
      console.error(`[sliderule-auth] Could not obtain token: ${err.message}`);
      console.error("[sliderule-auth] The SlideRule MCP server will be unavailable this session.");
      console.error("[sliderule-auth] Set SLIDERULE_MCP_TOKEN manually to skip this flow.");
      token = "";
    }
  }

  return {
    /**
     * Inject the token into every shell environment that opencode spawns,
     * so {env:SLIDERULE_MCP_TOKEN} in opencode.json resolves correctly and
     * any bash tool the agent runs also has the token available.
     */
    "shell.env": async (_input, output) => {
      if (token) {
        output.env.SLIDERULE_MCP_TOKEN = token;
      }
    },
  };
};
