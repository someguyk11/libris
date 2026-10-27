<template>
  <main ref="root" class="auth-view">
    <section class="auth-art" :style="{ backgroundImage: 'url(' + libraryArt + ')' }">
      <div class="auth-brand">
        <strong>Libris</strong>
      </div>
      <div class="auth-art__copy">
        <span>YOUR READING LIFE, IN ONE PLACE</span>
        <h1>Every book opens<br><em>another world.</em></h1>
        <p>Save the stories you love, follow thoughtful reading paths, and always know what to read next.</p>
      </div>
      <div class="auth-art__quote">
        <span class="material-symbols-rounded">format_quote</span>
        <p>A reader lives a thousand lives before he dies.</p>
      </div>
    </section>

    <section class="auth-panel">
      <div class="auth-panel__inner">
        <div class="auth-mobile-brand">
          <strong>Libris</strong>
        </div>

        <div class="auth-heading">
          <p>{{ mode === 'login' ? 'WELCOME BACK' : 'BEGIN YOUR LIBRARY' }}</p>
          <h2>{{ mode === 'login' ? 'Continue your story.' : 'Create your account.' }}</h2>
          <span>{{ mode === 'login' ? 'Sign in to return to your shelves.' : 'A quieter way to discover and remember books.' }}</span>
        </div>

        <div class="auth-tabs" role="tablist" aria-label="Account">
          <button :class="{ active: mode === 'login' }" @click="setMode('login')">Log in</button>
          <button :class="{ active: mode === 'signup' }" @click="setMode('signup')">Sign up</button>
        </div>

        <form ref="form" class="auth-form" @submit.prevent="submit">
          <Transition name="field">
            <label v-if="mode === 'signup' && !otpPhase && !recoveryPhase">
              <span>Your name</span>
              <div><i class="material-symbols-rounded">person</i><input v-model="name" :required="!apiMode" maxlength="80" autocomplete="name" placeholder="What should we call you?" /></div>
            </label>
          </Transition>

          <label v-if="mode === 'signup' && !otpPhase && !recoveryPhase">
            <span>Username <small>(optional)</small></span>
            <div><i class="material-symbols-rounded">alternate_email</i><input v-model="username" autocomplete="username" pattern="[A-Za-z0-9_]{3,30}" maxlength="30" placeholder="Choose a unique username" /></div>
          </label>

          <label v-if="recoveryPhase !== 'reset'">
            <span>Email address</span>
            <div><i class="material-symbols-rounded">mail</i><input v-model="email" :readonly="otpPhase" required type="email" autocomplete="email" placeholder="reader@example.com" /></div>
          </label>

          <label v-if="!otpPhase && (!recoveryPhase || recoveryPhase === 'reset')">
            <span>Password</span>
            <div>
              <i class="material-symbols-rounded">lock</i>
              <input v-model="password" required :type="showPassword ? 'text' : 'password'" :autocomplete="mode === 'login' && !recoveryPhase ? 'current-password' : 'new-password'" :placeholder="mode === 'signup' || recoveryPhase === 'reset' ? 'At least 12 characters' : 'Your password'" :minlength="apiMode ? (mode === 'signup' || recoveryPhase === 'reset' ? 12 : 1) : 8" maxlength="128" />
              <button type="button" aria-label="Show password" @click="showPassword = !showPassword">
                <span class="material-symbols-rounded">{{ showPassword ? 'visibility_off' : 'visibility' }}</span>
              </button>
            </div>
          </label>

          <label v-if="otpPhase"><span>Email verification code</span><div><i class="material-symbols-rounded">mark_email_read</i><input v-model="otp" inputmode="numeric" autocomplete="one-time-code" pattern="[0-9]{6,10}" required placeholder="Code from your email" /></div></label>
          <button v-if="otpPhase" type="button" :disabled="pending || resendSeconds || retrySeconds" @click="resend">{{ resendSeconds || retrySeconds ? `Resend in ${Math.max(resendSeconds, retrySeconds)}s` : 'Resend verification code' }}</button>
          <button v-if="otpPhase" type="button" :disabled="pending" @click="changeEmail">Use a different email</button>

          <div v-if="mode === 'login' && !recoveryPhase" class="auth-form__options">
            <span>Session expiry is managed securely.</span>
            <button type="button" :disabled="pending" @click="startRecovery">Forgot password?</button>
          </div>

          <button class="auth-submit" type="submit" :disabled="pending || retrySeconds">
            {{ pending ? 'Please wait…' : retrySeconds ? `Try again in ${retrySeconds}s` : recoveryPhase === 'reset' ? 'Set new password' : otpPhase ? 'Verify email' : recoveryPhase ? 'Send recovery code' : mode === 'login' ? 'Enter Libris' : 'Create account' }}
            <span class="material-symbols-rounded">arrow_forward</span>
          </button>

          <p v-if="message" class="auth-message" role="status">{{ message }}</p>
          <p v-if="apiMode && sessionEmail" class="auth-message">Signed in as {{ sessionEmail }} <button type="button" :disabled="pending" @click="logout">Sign out</button></p>
        </form>

        <a class="auth-guest-link" href="#/guide">Continue as guest — Book guidance</a>

        <div class="auth-divider"><span>or continue with</span></div>
        <button class="social-button" type="button" disabled>
          <strong>G</strong>
          Google · Coming soon
        </button>

        <p class="auth-switch">
          {{ mode === 'login' ? 'New to Libris?' : 'Already have an account?' }}
          <button @click="setMode(mode === 'login' ? 'signup' : 'login')">
            {{ mode === 'login' ? 'Create an account' : 'Log in' }}
          </button>
        </p>
      </div>
    </section>
  </main>
</template>

<script setup>
import { computed, ref, watch, nextTick, onMounted, onBeforeUnmount } from "vue"
import { gsap } from "@/animations/setup"
import { useNav } from "@/composables/useNav"
import useReducedMotion from "@/composables/useReducedMotion"
import libraryArt from "@/assets/library.png"
import { apiMode, authApi } from '@/services/api.js'
import { useSession } from '@/composables/useSession.js'
import { takeAuthCallback } from '@/services/authCallback.js'

const { current: activeNav, afterSignIn, authMode } = useNav()
const account = useSession()
const sessionEmail = computed(() => account.user.value?.email || '')
const recoveryPhase = ref('')
const isReducedMotion = useReducedMotion()
const root = ref(null)
const form = ref(null)
const mode = ref(authMode.value)
authMode.value = 'login'
const name = ref("")
const username = ref('')
const email = ref("")
const password = ref("")
const showPassword = ref(false)
const message = ref("")
const pending = ref(false), otpPhase = ref(false), otp = ref('')
let entrance = null
const clock = ref(Date.now()), retryUntil = ref(0), resendUntil = ref(0)
const retrySeconds = computed(() => Math.max(0, Math.ceil((retryUntil.value - clock.value) / 1000)))
const resendSeconds = computed(() => Math.max(0, Math.ceil((resendUntil.value - clock.value) / 1000)))
let ticker
function failed(cause) {
  message.value = cause.message
  if (cause.status === 429) retryUntil.value = Date.now() + (cause.retryAfter || 60) * 1000
  clock.value = Date.now()
}
function sent() { resendUntil.value = Date.now() + 30000; clock.value = Date.now() }
watch(() => account.session.value?.purpose, purpose => {
  if (purpose === 'recovery') { recoveryPhase.value = 'reset'; otpPhase.value = false }
}, { immediate: true })

onMounted(async () => {
  ticker = setInterval(() => { clock.value = Date.now() }, 1000)
  const callback = takeAuthCallback()
  if (apiMode && callback) {
    pending.value = true
    try {
      if (callback.error) { account.accept(null); message.value = callback.error }
      else {
        const value = await authApi.callback(callback)
        account.accept(value)
        if (value.purpose === 'recovery') { recoveryPhase.value = 'reset'; message.value = 'Email verified. Choose a new password.' }
        else { afterSignIn(); return }
      }
    } catch (cause) { account.accept(null); failed(cause) }
    finally { pending.value = false }
  }
  if (account.session.value?.purpose === 'recovery') recoveryPhase.value = 'reset'
  if (isReducedMotion.value) return
  entrance = gsap.timeline({ defaults: { ease: "power3.out" } })
    .fromTo(".auth-art", { opacity: 0 }, { opacity: 1, duration: .8 })
    .fromTo(".auth-art__copy > *", { opacity: 0, y: 16 }, { opacity: 1, y: 0, duration: .5, stagger: .08 }, .18)
    .fromTo(".auth-panel__inner", { opacity: 0, x: 14 }, { opacity: 1, x: 0, duration: .55 }, .24)
})
onBeforeUnmount(() => { entrance?.kill(); clearInterval(ticker) })

async function setMode(nextMode) {
  if (pending.value) return
  message.value = ""
  recoveryPhase.value = ''
  otpPhase.value = false; otp.value = ''; password.value = ''
  mode.value = nextMode
  await nextTick()
  if (!isReducedMotion.value) {
    gsap.fromTo(form.value, { opacity: .55, y: 6 }, { opacity: 1, y: 0, duration: .26, ease: "power2.out" })
  }
}
async function submit() {
  if (pending.value || retrySeconds.value) return
  if (apiMode) {
    pending.value = true; message.value = ''
    try {
      if (recoveryPhase.value === 'reset') { await authApi.resetPassword(password.value); account.accept(null); password.value = ''; recoveryPhase.value = ''; message.value = 'Password updated. Sign in with your new password.' }
      else if (otpPhase.value && recoveryPhase.value) { account.accept(await authApi.verifyRecovery(email.value, otp.value)); otp.value = ''; otpPhase.value = false; recoveryPhase.value = 'reset' }
      else if (recoveryPhase.value) { await authApi.forgotPassword(email.value); sent(); otpPhase.value = true; message.value = 'If eligible, a recovery email has been sent. Open its link or enter its code here.' }
      else if (otpPhase.value) { account.accept(await authApi.verify(email.value, otp.value)); password.value = ''; otp.value = ''; afterSignIn() }
      else if (mode.value === 'signup') { await authApi.signup(email.value, password.value, { display_name: name.value, username: username.value }); sent(); password.value = ''; otpPhase.value = true; message.value = 'If eligible, a verification email has been sent. Open its link or enter its code here.' }
      else { account.accept(await authApi.login(email.value, password.value)); password.value = ''; afterSignIn() }
    } catch (cause) { failed(cause) }
    finally { pending.value = false }
    return
  }
  message.value = mode.value === "login"
    ? "Demo sign-in complete — welcome back."
    : "Your demo account is ready. Welcome to Libris."
}
async function resend() {
  if (pending.value || resendSeconds.value || retrySeconds.value) return
  pending.value = true
  try { await (recoveryPhase.value ? authApi.forgotPassword(email.value) : authApi.resend(email.value)); sent(); message.value = 'If eligible, another verification email has been sent.' }
  catch (cause) { failed(cause) }
  finally { pending.value = false }
}
async function logout() {
  pending.value = true
  try { await account.signOut(); message.value = 'Signed out.' }
  catch (cause) { message.value = cause.message }
  finally { pending.value = false }
}
function startRecovery() {
  if (!apiMode) { message.value = 'Password recovery requires a connected account, not demo mode.'; return }
  recoveryPhase.value = 'email'; otpPhase.value = false; password.value = ''; message.value = 'Enter your account email to request a recovery code.'
}
function changeEmail() { otpPhase.value = false; otp.value = ''; message.value = ''; password.value = '' }
</script>

<style scoped>
.auth-view { display:grid; grid-template-columns:minmax(0,1.08fr) minmax(26rem,.92fr); min-height:100vh; background:#08060f; }
.auth-art {
  position:relative;
  isolation:isolate;
  display:flex;
  flex-direction:column;
  justify-content:space-between;
  min-height:100vh;
  padding:2.2rem clamp(2rem,5vw,5rem);
  overflow:hidden;
  background-size:cover;
  background-position:62% center;
}
.auth-art::before { content:""; position:absolute; inset:0; z-index:-1; background:linear-gradient(90deg,rgba(8,6,15,.96),rgba(8,6,15,.56) 60%,rgba(8,6,15,.2)),linear-gradient(0deg,rgba(8,6,15,.85),transparent 50%); }
.auth-brand, .auth-mobile-brand { display:flex; align-items:center; gap:.7rem; color:var(--text-primary); }
.auth-brand { align-self:flex-start; padding:0; border:0; background:transparent; cursor:pointer; }
.auth-brand span, .auth-mobile-brand span { display:grid; place-items:center; width:2.6rem; height:2.6rem; border:1px solid rgba(179,136,255,.3); border-radius:12px; color:var(--accent); background:rgba(15,10,27,.54); }
.auth-brand strong, .auth-mobile-brand strong { font:500 1.8rem var(--font-display); }
.auth-art__copy { max-width:36rem; padding-bottom:2rem; }
.auth-art__copy > span { color:var(--accent); font-size:.68rem; font-weight:700; letter-spacing:.18em; }
.auth-art__copy h1 { margin:1rem 0 1.2rem; color:var(--text-primary); font:500 clamp(3.1rem,5.4vw,5.5rem)/.88 var(--font-display); letter-spacing:-.035em; }
.auth-art__copy h1 em { color:var(--accent); font-weight:400; }
.auth-art__copy p { max-width:31rem; color:var(--text-secondary); font-size:.95rem; line-height:1.7; }
.auth-art__quote { display:flex; align-items:center; gap:.8rem; padding-top:1.25rem; border-top:1px solid rgba(243,240,234,.1); color:var(--text-secondary); }
.auth-art__quote span { color:var(--accent); }
.auth-art__quote p { font:italic 1rem var(--font-display); }
.auth-panel { display:grid; place-items:center; padding:2rem clamp(1.5rem,5vw,5rem); background:radial-gradient(circle at 50% 15%,rgba(82,40,138,.12),transparent 22rem),#0b0812; }
.auth-panel__inner { width:min(100%,27rem); }
.auth-panel__back { display:flex; align-items:center; gap:.4rem; margin-bottom:3rem; padding:0; border:0; color:var(--text-tertiary); background:transparent; font:500 .75rem var(--font-body); cursor:pointer; }
.auth-panel__back span { font-size:1rem; }
.auth-mobile-brand { display:none; }
.auth-heading > p { margin:0 0 .55rem; color:var(--accent); font-size:.62rem; font-weight:700; letter-spacing:.16em; }
.auth-heading h2 { margin:0; color:var(--text-primary); font:500 2.5rem/.98 var(--font-display); }
.auth-heading > span { display:block; margin-top:.75rem; color:var(--text-secondary); font-size:.82rem; }
.auth-tabs { display:grid; grid-template-columns:1fr 1fr; gap:.25rem; margin:2rem 0 1.7rem; padding:.28rem; border:1px solid var(--border-subtle); border-radius:12px; background:rgba(19,16,29,.7); }
.auth-tabs button { padding:.7rem; border:0; border-radius:9px; color:var(--text-tertiary); background:transparent; font:600 .78rem var(--font-body); cursor:pointer; transition:.2s ease; }
.auth-tabs button.active { color:var(--text-primary); background:linear-gradient(135deg,rgba(119,79,190,.72),rgba(83,48,146,.72)); box-shadow:0 5px 18px rgba(75,39,137,.2); }
.auth-form { display:flex; flex-direction:column; gap:1rem; }
.auth-form > label > span { display:block; margin-bottom:.4rem; color:var(--text-secondary); font-size:.72rem; font-weight:600; }
.auth-form > label > div { display:flex; align-items:center; height:3.05rem; padding:0 .8rem; border:1px solid var(--border-soft); border-radius:11px; background:rgba(15,12,24,.78); transition:.2s ease; }
.auth-form > label > div:focus-within { border-color:rgba(179,136,255,.55); box-shadow:0 0 0 3px rgba(179,136,255,.08); }
.auth-form i { margin-right:.65rem; color:var(--text-tertiary); font-size:1.05rem; }
.auth-form input { flex:1; min-width:0; border:0; outline:0; color:var(--text-primary); background:transparent; font:.8rem var(--font-body); }
.auth-form input:-webkit-autofill, .auth-form input:-webkit-autofill:hover, .auth-form input:-webkit-autofill:focus { -webkit-text-fill-color:var(--text-primary); -webkit-box-shadow:0 0 0 1000px rgba(15,12,24) inset; caret-color:var(--text-primary); transition:background-color 9999s ease-out; }
.auth-form input::placeholder { color:var(--text-disabled); }
.auth-form label button { display:grid; place-items:center; border:0; color:var(--text-tertiary); background:transparent; cursor:pointer; }
.auth-form label button span { font-size:1rem; }
.auth-form__options { display:flex; justify-content:space-between; align-items:center; }
.auth-form__options .remember { display:flex; align-items:center; gap:.4rem; color:var(--text-secondary); font-size:.7rem; }
.auth-form__options .remember input { accent-color:var(--accent); }
.auth-form__options button { border:0; color:var(--accent); background:transparent; font:600 .7rem var(--font-body); }
.auth-submit { display:flex; align-items:center; justify-content:center; gap:.65rem; height:3.05rem; margin-top:.25rem; border:1px solid rgba(202,171,255,.2); border-radius:11px; color:white; background:linear-gradient(135deg,#8058c8,#643ca8); box-shadow:0 10px 28px rgba(79,40,145,.24); font:650 .82rem var(--font-body); cursor:pointer; transition:transform .18s ease,box-shadow .18s ease; }
.auth-submit:hover { transform:translateY(-1px); box-shadow:0 13px 34px rgba(97,52,170,.34); }
.auth-submit span { font-size:1rem; }
.auth-message { padding:.7rem; border:1px solid rgba(126,168,138,.26); border-radius:9px; color:#a8cfb4; background:rgba(126,168,138,.08); font-size:.72rem; text-align:center; }
.auth-guest-link { display:flex; align-items:center; justify-content:center; min-height:44px; margin-top:1rem; padding:.65rem .8rem; border:1px solid var(--border-soft); border-radius:11px; color:var(--accent); background:rgba(16,13,25,.62); font:600 .76rem/1.5 var(--font-body); text-align:center; text-decoration:none; }
.auth-guest-link:hover { border-color:var(--accent); }
.auth-guest-link:focus-visible { outline:2px solid var(--accent); outline-offset:4px; }
.auth-divider { display:flex; align-items:center; gap:.7rem; margin:1.4rem 0; color:var(--text-disabled); font-size:.65rem; }
.auth-divider::before,.auth-divider::after { content:""; flex:1; height:1px; background:var(--border-subtle); }
.social-button { display:flex; align-items:center; justify-content:center; gap:.65rem; width:100%; height:2.85rem; border:1px solid var(--border-soft); border-radius:11px; color:var(--text-secondary); background:rgba(16,13,25,.62); font:600 .76rem var(--font-body); cursor:pointer; }
.social-button strong { color:var(--text-primary); font:600 .95rem Arial,sans-serif; }
.auth-switch { margin:1.5rem 0 0; color:var(--text-tertiary); font-size:.72rem; text-align:center; }
.auth-switch button { border:0; color:var(--accent); background:transparent; font:600 .72rem var(--font-body); cursor:pointer; }
.field-enter-active,.field-leave-active { transition:opacity .2s ease,transform .2s ease; }
.field-enter-from,.field-leave-to { opacity:0; transform:translateY(-6px); }
@media (max-width: 800px) {
  .auth-view { display:block; }
  .auth-art { display:none; }
  .auth-panel { min-height:100vh; padding:1.4rem 1.25rem 3rem; }
  .auth-panel__inner { width:min(100%,26rem); }
  .auth-panel__back { margin-bottom:2rem; }
  .auth-mobile-brand { display:flex; margin-bottom:3rem; }
  .auth-heading h2 { font-size:2.25rem; }
}
</style>
