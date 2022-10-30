## Piped

By default plugin only supports [piped.kavin.rocks](https://piped.kavin.rocks/) instance and works using it's corresponding API endpoint.

Additional hosts can be put in `piped_hosts` file in `gtuber` config directory.
If you want to use custom hosts, you must additionally create `piped_api_hosts` config file.
Put a list of API hosts there. It is expected that both frontend and API operate on different
subdomain of the same domain. That is how plugin will match video URI with API host to use.

List of available instances can be found here: [https://piped-instances.kavin.rocks/](https://piped-instances.kavin.rocks/)
