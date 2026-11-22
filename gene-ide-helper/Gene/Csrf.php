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
     * @param array|null $cfg optional override of the current path's csrf scope
     * @return string
     */
    public static function token($cfg = null) {}

    /**
     * Constant-time compare against the session token.
     * $token omitted → reads X-CSRF-Token header, then $_POST['_token'].
     * Read-only: a failed check never creates/rotates the session token.
     *
     * @param string|null $token
     * @param array|null $cfg optional override of the current path's csrf scope
     * @return bool
     */
    public static function check($token = null, $cfg = null) {}

    /**
     * Hidden form input: <input type="hidden" name="_token" value="...">.
     *
     * @param array|null $cfg optional override of the current path's csrf scope
     * @return string
     */
    public static function field($cfg = null) {}
}
