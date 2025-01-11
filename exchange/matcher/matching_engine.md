# The MatchingEngine class
A `MatchingEngine` instance will hold:
- a OrderBookHashMap instance, referred to as `ticker_order_book_`, holding an order book for every ticker_id (i.e., different type of market/stock/company).
- a queue of `incoming_requests_` from the order gateway server;
- a queue of `outgoing_ogw_responses_` for the order gateway server;
- a queue of `outgoing_md_updates_` for the market data publisher.

When `MatchingEngine::run()` starts, the code loops over all `incoming_requests_` from the order gateway server, calling `MatchingEngine::processClientRequest` on each of them.

During `MatchingEngine::processClientRequest`,
1. We obtain the `order_book` for its respective `ticker_id_` from `ticker_order_book_`.
2. We check the type of `MEClientRequest` it is:
- If `ClientRequestType::NEW`, we call `MEOrderBook::add`, to add the order from the `MEClientRequest` to its corresponding `order_book`;
- If `ClientRequestType::CANCEL`, we call `MEOrderBook::cancel` instead, and remove the order specified by the `MEClientRequest` from its corresponding `order_book`.

There are also other two methods: `MatchingEngine::sendClientResponse`, which takes a `MEClientResponse` from the matching engine and adds it to `outgoing_ogw_responses_` to be passed onto the order gateway server; and `MatchingEngine::sendMarketUpdate`, which takes a `MEMarketUpdate` from the matching engine and adds it to `outgoing_md_updates_` to be passed onto the market data publisher.

Both `MatchingEngine::sendClientResponse` and `MatchingEngine::sendMarketUpdate` methods are called during `MEOrderBook::add`, `MEOrderBook::cancel` and `MEOrderBook::match`, through the MEOrderBook's internal `matching_engine_` instance. 