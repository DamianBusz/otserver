# Market tier overview: extended opcode 91, version 1

The updated OTClient market shows tier cards for a selected upgradeable item.
Each card shows the lowest current sell price, buy-only availability or an
explicit empty book. Clicking a card browses that tier using the existing
market action without clearing search text. Locker quantities remain distinct
from public listings.

## Solicited snapshot

The client sends one debounced, read-only request after selecting an item or
pressing Refresh. No capability is inferred from another extension or a client
version. Extended opcode **91** is reserved in this distribution:

```json
{"version":1,"action":"tiers","itemId":58388,"request":"0123456789abcdef0123456789abcdef"}
```

The server requires an authenticated, current player connection in the market,
native market support, a non-legacy connection and a valid market item with an
upgrade classification. Viewers, detached connections, malformed inputs, invalid
IDs, unsupported versions and requests above 256 bytes are rejected. Requests
are limited to one aggregate lookup per 300 ms per connection.

A single PostgreSQL aggregate groups unexpired, positive-amount listings for
the requested item by tier and buy/sell side. It returns offer counts, total
quantities, the minimum sell price and maximum buy price. Only tiers 0 through
the configured maximum (capped at the client's supported tier 10) are queried.
An aggregate always returns one database row: a failed query remains distinct
from a successfully queried empty book.

```json
{"version":1,"ok":true,"itemId":58388,"request":"0123456789abcdef0123456789abcdef","maxTier":10,"tiers":[{"tier":0,"sellOffers":3,"buyOffers":0,"sellAmount":"5","buyAmount":"0","bestSell":"45000000","bestBuy":"0"}]}
```

Successful responses contain every tier from 0 through maxTier, including empty
tiers. Price and quantity fields are exact decimal strings; offer counts are
integers. A database failure returns `ok:false` with no tiers. Output uses
game opcode `0x32`, extended opcode `91`, a u16 JSON length and raw JSON bytes.
It bypasses livestream mirroring. There are no native packet tails, player
names or private balances in the summary.

The client validates the whole snapshot before replacing state and rejects
responses for an old item, request token or protocol connection. Closing the
market cancels pending requests. Old servers and failed/timed-out requests show
availability as unknown; manual tier browsing remains available. Previously
received prices can remain visible while Refresh is pending.

## Trade safety

Anonymous offering mode is disabled. OTClient does not expose the checkbox and
always sends a zero anonymity flag in the native packet. Canary retains the
legacy byte/API parameter for compatibility but stores new offers with
`anonymous=0` regardless of that input. All active-offer readers and the
counter lookup return the owner's name, including orders previously created
anonymously. Existing orders remain tradable without a database rewrite.

Snapshots are discovery information. Accept/create/cancel actions use the
existing server-validated native market protocol. No purchase uses summary
prices or bypasses the existing confirmation flow.

OTClient's native market reader preserves the item ID and tier in its Lua
callbacks even for empty books. Late browse/detail responses are discarded if
the selected item/tier changed, and selecting another tier clears the previous
offer selection and disables Accept before requesting its book.

## Verification

`MarketTierOverviewTest.*` checks bounded requests, exact prices, empty/failure
semantics, all-tier serialization and the following opcode boundary. The test
exports its production aggregate SQL and payload as GoogleTest XML properties.
An isolated PostgreSQL regression executes that exported SQL against fixture
listings. OTClient additionally tests receiver validation, cancellation, stale
responses and the real Sanguine Rod market UI.

Deploy the matching server binary and client before expecting live summaries.
Host builds do not replace a running Docker server.
