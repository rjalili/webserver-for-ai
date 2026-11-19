<?php
use Illuminate\Support\Facades\Route;
Route::get('/', [HomeController::class, 'index']);
Route::get('/shop/catalogue', [ShopController::class, 'index']);
Route::get('/shop/basket', [BasketController::class, 'show'])->middleware('auth');
Route::middleware(['auth', 'verified'])->prefix('member')->group(function () {
    Route::get('/invoices', [InvoiceController::class, 'index']);
    Route::get('/invoices/{id}', [InvoiceController::class, 'show']);
});
Route::post('/newsletter', [NewsletterController::class, 'store']);
