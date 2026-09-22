/* حالة الكاش لمتصفح PS5 (AppCache) */
(function () {
  var box = document.getElementById("cache-status");
  if (!box) {
    box = document.createElement("p");
    box.id = "cache-status";
    (document.querySelector(".top") || document.body).appendChild(box);
  }
  function setmsg(t, c) { box.textContent = t; if (c) box.style.color = c; }
  function online() { try { return navigator.onLine !== false; } catch (e) { return true; } }

  var ac = window.applicationCache;
  if (!ac) { setmsg("وضع الأوفلاين غير مدعوم في هذا المتصفح"); return; }

  setmsg(online() ? "كاش: جاري الفحص..." : "كاش: أوفلاين — الاتصال بالكاش...");

  ac.addEventListener("checking", function () { if (online()) setmsg("كاش: جاري الفحص..."); });
  ac.addEventListener("downloading", function () { setmsg("كاش: جاري التحميل... لا تغلق الصفحة", "#ffcc66"); });
  ac.addEventListener("progress", function (e) {
    if (e && e.total && e.loaded) setmsg("كاش: " + e.loaded + " / " + e.total + " — لا تغلق الصفحة", "#ffcc66");
    else setmsg("كاش: جاري التحميل...", "#ffcc66");
  });
  ac.addEventListener("cached", function () { setmsg("تم حفظ الكاش. الآن اقطع النت وأعد فتح نفس الرابط — جاهز.", "#39ff14"); });
  ac.addEventListener("noupdate", function () {
    if (online()) setmsg("الكاش موجود وجاهز يفتح بدون نت.", "#39ff14");
    else setmsg("وضع الأوفلاين — الكاش يعمل.", "#39ff14");
  });
  ac.addEventListener("error", function () {
    if (!online()) {
      setmsg("وضع الأوفلاين — الكاش يعمل، الصفحة تُحمَّل من الجهاز.", "#39ff14");
    } else {
      setmsg("خطأ في الكاش: أول زيارة يجب أن تكون بالنت، ولا تمسح بيانات المتصفح.", "#ff5566");
    }
  });
})();