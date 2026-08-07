<?php
namespace Gene;

/**
 * Benchmark
 * 
 * @author  sasou<admin@php-gene.com>
 * @version  5.4.3
 */
 
class Benchmark
{

    /**
     * start
     * 
     * @return mixed
     */
    public static function start() {

    }

    /**
     * end
     * 
     * @return mixed
     */
    public static function end() {

    }

    /**
     * time
     * 
     * @param bool $type type
     * @return mixed
     */
    public static function time($type = false) {

    }

    /**
     * memory
     * 
     * @param bool $type type
     * @return mixed
     */
    public static function memory($type = false) {

    }

    /**
     * mark
     * Record a named high-resolution timestamp for later lap() measurement.
     *
     * @param string $name mark name
     * @return bool
     */
    public static function mark(string $name) {

    }

    /**
     * lap
     * Return milliseconds (float) elapsed since the last mark($name),
     * then reset the mark to now. Returns false if no prior mark exists.
     *
     * @param string $name mark name
     * @return float|false
     */
    public static function lap(string $name) {

    }

}