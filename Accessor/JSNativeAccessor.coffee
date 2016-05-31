console.log 'JSNativeAccessor ... ...'

# console.log new JSNativeAccessor.Void(123).pointer.value

# JSNativeAccessor.typedef {
#   width: JSNativeAccessor.Float
#   height: JSNativeAccessor.Float

{FFI, STDC, Buffer} = NativeAccessor

isalnum = do ({isalnum} = STDC.type, cb = ->)->
  isalnum.call = new FFI.Signature FFI.BuildIn.Int, FFI.BuildIn.Int
  return (num)->
    return false unless typeof num is 'number'
    rv = new FFI.BuildIn.Int 0
    num = new FFI.BuildIn.Int num
    isalnum.call rv, num , cb
    return rv.getValue() isnt 0

console.log "#{num} is #{if isalnum num then '' else 'not'} a Number" for num in [0...128]