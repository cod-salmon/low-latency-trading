# Client Requests
In the `client_request.h` file we have the following:
- A **client request**: A definition of the `MEClientRequest` struct. Every instance of this struct will hold specific values for most typedefs with defined in `common/types.h`. 
- A **to-string method for the client request**. The `MEClientRequest` struct has a `toString`  method to print itself (the `common/types.h` values that characterise it).
- A **client request type**. An enum class called `ClientRequestType` which can be either INVALID, NEW (the client has a new order request) or CANCEL (the client wishes to cancel a buy/sell order request).
- A **to-string method for the client request type**. 

We also define a new type of container called `ClientRequestLFQueue`, which is an `LFQueue` (see `common/lf_queue.h`) holding `MEClientRequest` objects.

# Client Responses
In the `client_response.h` file we have the following:
- A **client response**: A definition of the `MEClientResponse` struct. Compared to `MEClientRequest`, `MEClientResponse` has two `OrderId` instances and two `Qty` instances:
    - `MEClientRequest` has `order_id` (referring to the client order id); while `MEClientResponse` has `client_order_id`, to distinguish it from `market_order_id`.
    - `MEClientRequest` has `qty_` (referring to quantity the client wants to buy/sell); while `MEClientResponse` has `exec_qty_` and `leaves_qty_`, referring to the final execution and remaining quantities after the execution.
- A **to-string method for the client response**. The `MEClientResponse` struct has a `toString`  method to print itself.
- A **client response type**. An enum class called `ClientResponseType` which can be 
    - ACCEPTED - the request has been accepted by the matching engine; 
    - CANCELED - the matching engine has successfully cancelled the order the  client requested.
    - FILLED - the matching engine has been able to successfully fill the client's request (buy/sell);
    - CANCEL_REJECTED - when the cancel request from the client comes too late; and 
    - INVALID.
- A **to-string method for the client request type**. 

We also define a new type of container called `ClientResponseLFQueue`, which is an `LFQueue` (see `common/lf_queue.h`) holding `MEClientResponse` objects.