<?php
namespace Api;

class Ping extends \Gene\Controller
{
    public function pong()
    {
        return $this->data([
            'pong' => true,
            'name' => \Gene\Request::post('name'),
        ]);
    }
}
