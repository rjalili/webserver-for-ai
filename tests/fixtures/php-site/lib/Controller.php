<?php
class Controller {
    public function handle() { if (!Auth::check()) { return redirect('/login'); } }
}
