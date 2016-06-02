console.log 'JSNativeAccessor ... ...'

# console.log new JSNativeAccessor.Void(123).pointer.value

# JSNativeAccessor.typedef {
#   width: JSNativeAccessor.Float
#   height: JSNativeAccessor.Float

{FFI, STDC, Buffer, typedef} = NativeAccessor

# isalnum = do ({isalnum} = STDC.type, cb = ->)->
#   isalnum.call = new FFI.Signature FFI.BuildIn.Int, FFI.BuildIn.Int
#   return (num)->
#     return false unless typeof num is 'number'
#     rv = new FFI.BuildIn.Int 0
#     num = new FFI.BuildIn.Int num
#     isalnum.call rv, num , cb
#     return rv.getValue() isnt 0

# console.log "#{num} is #{if isalnum num then '' else 'not'} a Number" for num in [0...128]

# count = 100000
# console.log 'start ...'
# start = new Date().getTime()

# # while count-- then new Date
# "#{num} is #{if isalnum num then '' else 'not'} a Number" for num in [0...128]

# end = new Date().getTime()

# console.log "total time: #{end - start}"


sig = new FFI.Signature FFI.BuildIn.Void

Callback = new sig

cb = new Callback ->console.log 'JavaScript Function for C Method from JavaScript'

Echo = new FFI.BuildIn.Pointer()
Echo.setValue(echo)

Echo.call = new FFI.Signature FFI.BuildIn.Void, Callback

Echo.call new FFI.BuildIn.Void, cb, ->

# console.log JSON.stringify ("#{key}": (key for key of val) for key, val of STDC), null, 2
print = do ({printf} = STDC.io, {Signature, BuildIn} = FFI, cb = ->)->
  printf.call = new Signature BuildIn.Void, BuildIn.Pointer
  return (args ...)->
    content = (JSON.stringify arg, null, 2 for arg in args).join(',') + '\n'
    rv = new BuildIn.Void
    av = new BuildIn.Pointer new Buffer(content)
    printf.call rv, av, cb

print 'log message from c printf'
