# Akyuu Privacy Notice

Last updated: October 1, 2026

This notice describes how Akyuu handles information when you use the application. Akyuu is a desktop application. It stores its settings, anime library, and account information on your device.

## Information stored on your device

Akyuu stores application settings, library data, and service account details in its local data directory. Account details can include usernames, email addresses, passwords for services that use them, and OAuth access or refresh tokens.

The account file is stored as readable JSON and is not encrypted by Akyuu. Akyuu creates it with owner-only (`0600`) file permissions on supported systems. Anyone or any process running as your user can still read it.

## Information sent to other services

When you connect a service or synchronize your library, Akyuu sends the authentication information, requests, and library changes needed to use the service you selected. The service can receive information about your account and the anime entries you request, add, update, or delete. Your chosen service's own privacy terms apply to information it receives.

Other features make network requests to the destinations they use: Akyuu checks GitHub for release information, retrieves configured feeds and torrent search results, and loads images from their source URLs. These destinations receive ordinary request information, such as your network address and requested URL.

If you enable Discord Rich Presence or configure IRC or HTTP sharing, Akyuu sends the watched anime and episode details included by your sharing settings to the destinations you configured. Custom-formatted fields you enable may also be included. Akyuu may open torrent links or local media in other applications you chose; those applications then receive the link or local file they are asked to open.

## MyAnimeList sign-in

Choosing **Accept and continue** in the MyAnimeList notice opens MyAnimeList sign-in in your web browser. MyAnimeList handles your sign-in and consent. Akyuu does not ask you to enter your MyAnimeList password into Akyuu.

After you authorize Akyuu, MyAnimeList redirects your browser to the Akyuu code page hosted at `cenky.dev`. The page displays an authorization code for you to copy into Akyuu. The redirect request contains that code and normal web request details. The `cenky.dev` server may log the request's IP address, time, and full URL, including the authorization code. This notice does not establish whether a particular request was logged or how long server logs are retained.

When you submit the code in Akyuu, the application sends it and its PKCE verifier to MyAnimeList to obtain account tokens. Akyuu stores the resulting tokens in its local account file as described above.

## Your choices

You can cancel the MyAnimeList notice without opening the sign-in page. You can also choose not to connect an external service or use network features. Disconnecting a service in Akyuu does not itself delete data already held by that service; consult that service for its account and data controls.

For privacy questions, contact [cenkkgl@gmail.com](mailto:cenkkgl@gmail.com).
