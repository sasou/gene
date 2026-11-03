<?php
namespace Gene;

/**
 * Synchronizer-token CSRF helpers, backed by the `session` DI component.
 * The central policy (Fetch Metadata + Origin/Referer + token) is enforced
 * by Application::csrf(); these methods are the manual surface.
 */
final class Csrf
{
    /**
     * Session-backed token (created on first use, stable for the session).
     * Render it into forms via field() or read it for X-CSRF-Token headers.
     *
     * @return string
     */
    public static function token() {}

    /**
     * Constant-time compare against the session token.
     * $token omitted → reads X-CSRF-Token header, then $_POST['_token'].
     * Read-only: a failed check never creates/rotates the session token.
     *
     * @param string|null $token
     * @return bool
     */
    public static function check($token = null) {}

    /**
     * Hidden form input: <input type="hidden" name="_token" value="...">.
     *
     * @return string
     */
    public static function field() {}
}
